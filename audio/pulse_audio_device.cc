#include "pulse_audio_device.h"

#include <iostream>
#include <chrono>
#include <thread>

namespace media {

namespace {
// Wait timeout in milliseconds
constexpr int kPulseOperationTimeoutMs = 5000;

// Helper function to wait for an operation to complete
[[maybe_unused]] void WaitForOperation(pa_operation* op, pa_mainloop* mainloop) {
    if (!op) return;
    
    auto start_time = std::chrono::steady_clock::now();
    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        pa_mainloop_iterate(mainloop, 1, nullptr);
        
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time).count();
        
        if (elapsed_ms > kPulseOperationTimeoutMs) {
            std::cerr << "PulseAudio operation timed out." << std::endl;
            break;
        }
    }
    pa_operation_unref(op);
}
}  // namespace

std::unique_ptr<PulseAudioDevice> PulseAudioDevice::Create(
    const PulseAudioDeviceConfig& config) {
    auto device = std::unique_ptr<PulseAudioDevice>(new PulseAudioDevice(config));
    if (!device->Initialize()) {
        return nullptr;
    }
    return device;
}

PulseAudioDevice::PulseAudioDevice(const PulseAudioDeviceConfig& config)
    : config_(config) {}

PulseAudioDevice::~PulseAudioDevice() {
    if (stream_) {
        pa_stream_disconnect(stream_);
        pa_stream_unref(stream_);
    }
    
    if (context_) {
        pa_context_disconnect(context_);
        pa_context_unref(context_);
    }
    
    if (mainloop_) {
        pa_mainloop_free(mainloop_);
    }
}

bool PulseAudioDevice::Initialize() {
    // Create a mainloop API and connection to the default server
    mainloop_ = pa_mainloop_new();
    if (!mainloop_) {
        std::cerr << "Failed to create PulseAudio mainloop." << std::endl;
        return false;
    }
    
    mainloop_api_ = pa_mainloop_get_api(mainloop_);
    
    context_ = pa_context_new(mainloop_api_, "PulseAudioDevice");
    if (!context_) {
        std::cerr << "Failed to create PulseAudio context." << std::endl;
        return false;
    }
    
    // Connect to the PulseAudio server
    pa_context_connect(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
    
    // Set the state callback
    pa_context_set_state_callback(context_, ContextStateCallback, this);
    
    // Wait for the context to be ready
    while (true) {
        pa_context_state_t state = pa_context_get_state(context_);
        if (state == PA_CONTEXT_READY) {
            break;
        }
        
        if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
            std::cerr << "Failed to connect to PulseAudio server." << std::endl;
            return false;
        }
        
        pa_mainloop_iterate(mainloop_, 1, nullptr);
    }
    
    // Set up the sample specification
    pa_sample_spec sample_spec;
    sample_spec.format = PA_SAMPLE_S16LE;
    sample_spec.rate = config_.sample_rate;
    sample_spec.channels = config_.channels;
    
    // Create the recording stream
    stream_ = pa_stream_new(context_, "record", &sample_spec, nullptr);
    if (!stream_) {
        std::cerr << "Failed to create PulseAudio stream." << std::endl;
        return false;
    }
    
    // Set the stream state callback
    pa_stream_set_state_callback(stream_, StreamStateCallback, this);
    
    // Set the stream read callback
    pa_stream_set_read_callback(stream_, StreamReadCallback, this);
    
    // Calculate buffer attributes based on config
    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = (config_.sample_rate * config_.channels * 2 * config_.buffer_ms) / 1000;
    buffer_attr.fragsize = buffer_attr.maxlength;
    buffer_attr.prebuf = (uint32_t)-1;
    buffer_attr.minreq = (uint32_t)-1;
    buffer_attr.tlength = (uint32_t)-1;
    
    // Connect the stream for recording
    pa_stream_flags_t flags = 
        static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE);
    
    // If device_id is empty, use monitor of default output device
    const char* source_name = nullptr;  // Default source
    if (!config_.device_id.empty()) {
        source_name = config_.device_id.c_str();
    }
    
    if (pa_stream_connect_record(stream_, source_name, &buffer_attr, flags) < 0) {
        std::cerr << "Failed to connect PulseAudio stream for recording." << std::endl;
        return false;
    }
    
    // Wait for the stream to be ready
    while (true) {
        pa_stream_state_t state = pa_stream_get_state(stream_);
        if (state == PA_STREAM_READY) {
            break;
        }
        
        if (state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED) {
            std::cerr << "Failed to connect PulseAudio stream." << std::endl;
            return false;
        }
        
        pa_mainloop_iterate(mainloop_, 1, nullptr);
    }
    
    // Pre-allocate buffer based on expected size
    size_t expected_bytes = (config_.sample_rate * config_.channels * 2 * config_.buffer_ms) / 1000;
    buffer_.reserve(expected_bytes);
    
    return true;
}

bool PulseAudioDevice::GetFrameS16LE(std::vector<uint8_t>* audio_data) {
    if (!audio_data || !stream_ || !mainloop_) {
        return false;
    }
    
    buffer_.clear();
    buffer_ready_ = false;
    
    // Process events until we get data or timeout
    auto start_time = std::chrono::steady_clock::now();
    while (!buffer_ready_) {
        pa_mainloop_iterate(mainloop_, 0, nullptr);
        
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time).count();
        
        if (elapsed_ms > kPulseOperationTimeoutMs) {
            std::cerr << "Timeout waiting for audio data." << std::endl;
            return false;
        }
        
        // Short sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    *audio_data = buffer_;
    return true;
}

void PulseAudioDevice::StreamReadCallback(
    pa_stream* stream, [[maybe_unused]] size_t nbytes, void* userdata) {
    PulseAudioDevice* device = static_cast<PulseAudioDevice*>(userdata);
    
    // Read data from the stream
    const void* data;
    size_t bytes;
    if (pa_stream_peek(stream, &data, &bytes) < 0) {
        std::cerr << "Failed to read from PulseAudio stream." << std::endl;
        return;
    }
    
    if (bytes > 0 && data != nullptr) {
        // Copy data to our buffer
        const uint8_t* byte_data = static_cast<const uint8_t*>(data);
        device->buffer_.insert(device->buffer_.end(), byte_data, byte_data + bytes);
        device->buffer_ready_ = true;
    }
    
    // Mark data as read
    pa_stream_drop(stream);
}

void PulseAudioDevice::StreamStateCallback(pa_stream* stream, [[maybe_unused]] void* userdata) {
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_READY:
            std::cout << "PulseAudio stream ready." << std::endl;
            break;
        case PA_STREAM_FAILED:
            std::cerr << "PulseAudio stream error: " << 
                pa_strerror(pa_context_errno(pa_stream_get_context(stream))) << std::endl;
            break;
        case PA_STREAM_TERMINATED:
            std::cout << "PulseAudio stream terminated." << std::endl;
            break;
        default:
            break;
    }
}

void PulseAudioDevice::ContextStateCallback(pa_context* context, [[maybe_unused]] void* userdata) {
    switch (pa_context_get_state(context)) {
        case PA_CONTEXT_READY:
            std::cout << "PulseAudio context ready." << std::endl;
            break;
        case PA_CONTEXT_FAILED:
            std::cerr << "PulseAudio context error: " << 
                pa_strerror(pa_context_errno(context)) << std::endl;
            break;
        case PA_CONTEXT_TERMINATED:
            std::cout << "PulseAudio context terminated." << std::endl;
            break;
        default:
            break;
    }
}

}  // namespace media
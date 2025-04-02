#include "wasapi_audio_device.h"

#include <Functiondiscoverykeys_devpkey.h>
#include <avrt.h>
#include <mmreg.h>
#include <algorithm>
#include <cstring>

namespace media {

// Helper function to convert WAVEFORMATEX to a string for debugging
std::string FormatToString(WAVEFORMATEX* format) {
  if (!format) return "null";
  
  std::string result = "WAVEFORMATEX { ";
  result += "wFormatTag=" + std::to_string(format->wFormatTag) + ", ";
  result += "nChannels=" + std::to_string(format->nChannels) + ", ";
  result += "nSamplesPerSec=" + std::to_string(format->nSamplesPerSec) + ", ";
  result += "nAvgBytesPerSec=" + std::to_string(format->nAvgBytesPerSec) + ", ";
  result += "nBlockAlign=" + std::to_string(format->nBlockAlign) + ", ";
  result += "wBitsPerSample=" + std::to_string(format->wBitsPerSample) + ", ";
  result += "cbSize=" + std::to_string(format->cbSize) + " }";
  return result;
}

// Helper function to convert HRESULT to human-readable error message
std::string HResultToString(HRESULT hr) {
  char buffer[256];
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                buffer, sizeof(buffer), nullptr);
  return std::string(buffer);
}

// Factory method to create a new WASAPIAudioDevice
std::unique_ptr<WASAPIAudioDevice> WASAPIAudioDevice::Create(
    const WASAPIAudioDeviceConfig& config) {
  std::unique_ptr<WASAPIAudioDevice> device(new WASAPIAudioDevice(config));
  if (!device->Initialize()) {
    return nullptr;
  }
  return device;
}

// Constructor
WASAPIAudioDevice::WASAPIAudioDevice(const WASAPIAudioDeviceConfig& config)
    : config_(config),
      sample_rate_(config.sample_rate),
      channels_(config.channels),
      bits_per_sample_(16),
      frame_size_(0),
      buffer_frame_count_(0),
      device_enumerator_(nullptr),
      audio_device_(nullptr),
      audio_client_(nullptr),
      capture_client_(nullptr),
      com_initialized_(false) {}

// Destructor
WASAPIAudioDevice::~WASAPIAudioDevice() {
  Cleanup();
}

// Initialize the WASAPI audio device
bool WASAPIAudioDevice::Initialize() {
  // Initialize COM
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    return false;
  }
  com_initialized_ = true;

  // Create device enumerator
  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                      __uuidof(IMMDeviceEnumerator),
                      reinterpret_cast<void**>(&device_enumerator_));
  if (FAILED(hr)) {
    Cleanup();
    return false;
  }

  // Get the audio device
  if (config_.device_id.empty()) {
    // Use default device
    hr = device_enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &audio_device_);
  } else {
    // Use the specified device
    hr = device_enumerator_->GetDevice(
        reinterpret_cast<LPCWSTR>(config_.device_id.c_str()), &audio_device_);
  }
  if (FAILED(hr)) {
    Cleanup();
    return false;
  }

  // Create audio client
  hr = audio_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                             nullptr, reinterpret_cast<void**>(&audio_client_));
  if (FAILED(hr)) {
    Cleanup();
    return false;
  }

  // Get mix format
  WAVEFORMATEX* mix_format = nullptr;
  hr = audio_client_->GetMixFormat(&mix_format);
  if (FAILED(hr)) {
    Cleanup();
    return false;
  }

  // Calculate buffer duration
  REFERENCE_TIME buffer_duration = config_.buffer_ms * 10000; // 100ns units
  
  // Configure audio client for loopback capture
  hr = audio_client_->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_LOOPBACK,
      buffer_duration,
      0,
      mix_format,
      nullptr);
  
  if (FAILED(hr)) {
    CoTaskMemFree(mix_format);
    Cleanup();
    return false;
  }

  // Update with actual format properties
  sample_rate_ = mix_format->nSamplesPerSec;
  channels_ = mix_format->nChannels;
  bits_per_sample_ = mix_format->wBitsPerSample;
  
  // Free mix format
  CoTaskMemFree(mix_format);
  
  // Get buffer size
  hr = audio_client_->GetBufferSize(&buffer_frame_count_);
  if (FAILED(hr)) {
    Cleanup();
    return false;
  }
  
  // Create capture client
  hr = audio_client_->GetService(__uuidof(IAudioCaptureClient),
                               reinterpret_cast<void**>(&capture_client_));
  if (FAILED(hr)) {
    Cleanup();
    return false;
  }

  // Start audio capture
  hr = audio_client_->Start();
  if (FAILED(hr)) {
    Cleanup();
    return false;
  }

  // Calculate frame size and reserve buffer
  frame_size_ = channels_ * (bits_per_sample_ / 8);
  raw_buffer_.reserve(buffer_frame_count_ * frame_size_);
  
  return true;
}

// Clean up resources
void WASAPIAudioDevice::Cleanup() {
  if (audio_client_) {
    audio_client_->Stop();
  }
  
  if (capture_client_) {
    capture_client_->Release();
    capture_client_ = nullptr;
  }
  
  if (audio_client_) {
    audio_client_->Release();
    audio_client_ = nullptr;
  }
  
  if (audio_device_) {
    audio_device_->Release();
    audio_device_ = nullptr;
  }
  
  if (device_enumerator_) {
    device_enumerator_->Release();
    device_enumerator_ = nullptr;
  }

  if (com_initialized_) {
    CoUninitialize();
    com_initialized_ = false;
  }
}

// Capture a raw audio frame
bool WASAPIAudioDevice::CaptureAudioFrame() {
  if (!capture_client_) {
    return false;
  }

  // Wait for data to be available
  Sleep(config_.buffer_ms / 2);
  
  UINT32 packet_length = 0;
  BYTE* data;
  DWORD flags;
  UINT64 position;
  HRESULT hr;
  
  raw_buffer_.clear();

  while (true) {
    // Get next packet size
    hr = capture_client_->GetNextPacketSize(&packet_length);
    if (FAILED(hr)) {
      return false;
    }
    
    // Exit if no more packets
    if (packet_length == 0) {
      break;
    }
    
    // Get the packet
    hr = capture_client_->GetBuffer(&data, &packet_length, &flags, &position, nullptr);
    if (FAILED(hr)) {
      return false;
    }
    
    // Skip silent packets
    if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
      // Append data to buffer
      size_t offset = raw_buffer_.size();
      raw_buffer_.resize(offset + packet_length * frame_size_);
      std::memcpy(raw_buffer_.data() + offset, data, packet_length * frame_size_);
    } else {
      // Add silent samples
      size_t offset = raw_buffer_.size();
      raw_buffer_.resize(offset + packet_length * frame_size_, 0);
    }
    
    // Release the buffer
    hr = capture_client_->ReleaseBuffer(packet_length);
    if (FAILED(hr)) {
      return false;
    }
  }
  
  return !raw_buffer_.empty();
}

// Get audio frame and convert to signed 16-bit PCM
bool WASAPIAudioDevice::GetFrameS16LE(std::vector<uint8_t>* audio_data) {
  if (!audio_data) {
    return false;
  }
  
  // Capture raw audio
  if (!CaptureAudioFrame()) {
    return false;
  }

  // If format is already S16LE, just copy the data
  if (bits_per_sample_ == 16) {
    *audio_data = raw_buffer_;
    return true;
  }
  
  // Convert from float (32-bit) to S16LE
  if (bits_per_sample_ == 32) {
    const float* src = reinterpret_cast<const float*>(raw_buffer_.data());
    size_t frames = raw_buffer_.size() / (channels_ * sizeof(float));
    
    // Resize destination buffer
    audio_data->resize(frames * channels_ * sizeof(int16_t));
    int16_t* dst = reinterpret_cast<int16_t*>(audio_data->data());
    
    // Convert each sample
    for (size_t i = 0; i < frames * channels_; ++i) {
      float sample = src[i];
      // Clamp to [-1.0, 1.0]
      sample = std::max(-1.0f, std::min(1.0f, sample));
      // Convert to INT16 range
      dst[i] = static_cast<int16_t>(sample * 32767.0f);
    }
    
    return true;
  }
  
  // Unsupported format
  return false;
}

}  // namespace media
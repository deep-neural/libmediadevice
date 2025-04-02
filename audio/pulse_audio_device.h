#ifndef MEDIA_PULSE_AUDIO_DEVICE_H_
#define MEDIA_PULSE_AUDIO_DEVICE_H_

#include <memory>
#include <string>
#include <vector>
#include <pulse/pulseaudio.h>

namespace media {

struct PulseAudioDeviceConfig {
    int sample_rate;
    int channels;
    int buffer_ms;
    std::string device_id;  // Empty string means default device
};

class PulseAudioDevice {
public:
    // Creates and initializes a PulseAudioDevice for capturing system audio
    static std::unique_ptr<PulseAudioDevice> Create(const PulseAudioDeviceConfig& config);

    // Destructor to clean up PulseAudio resources
    ~PulseAudioDevice();

    // Gets a frame of audio data in signed 16-bit little-endian format
    // Returns true on success, false on failure
    bool GetFrameS16LE(std::vector<uint8_t>* audio_data);

    // Returns current configuration
    const PulseAudioDeviceConfig& GetConfig() const { return config_; }

private:
    // Private constructor, use Create() factory method instead
    explicit PulseAudioDevice(const PulseAudioDeviceConfig& config);

    // Initializes PulseAudio connection and stream
    bool Initialize();

    // Callback for when new audio data is available
    static void StreamReadCallback(pa_stream* stream, size_t nbytes, void* userdata);

    // Callback for stream state changes
    static void StreamStateCallback(pa_stream* stream, void* userdata);

    // Callback for context state changes
    static void ContextStateCallback(pa_context* context, void* userdata);

    // Configuration parameters
    PulseAudioDeviceConfig config_;

    // PulseAudio objects
    pa_mainloop* mainloop_ = nullptr;
    pa_mainloop_api* mainloop_api_ = nullptr;
    pa_context* context_ = nullptr;
    pa_stream* stream_ = nullptr;

    // Buffer for collecting audio data
    std::vector<uint8_t> buffer_;
    bool buffer_ready_ = false;
};

}  // namespace media

#endif  // MEDIA_PULSE_AUDIO_DEVICE_H_
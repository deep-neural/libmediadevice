#ifndef WASAPI_AUDIO_DEVICE_H_
#define WASAPI_AUDIO_DEVICE_H_

#define NOMINMAX  // Prevent Windows headers from defining min/max macros

// Include necessary Windows headers
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdint.h>

#include <string>
#include <memory>
#include <vector>

namespace media {

// Configuration for WASAPI audio device
struct WASAPIAudioDeviceConfig {
  std::string device_id;
  int sample_rate = 44100;
  int channels = 2;
  int buffer_ms = 100;
};

class WASAPIAudioDevice {
 public:
  // Factory method to create a device
  static std::unique_ptr<WASAPIAudioDevice> Create(const WASAPIAudioDeviceConfig& config);
  
  // Destructor
  ~WASAPIAudioDevice();

  // Get audio frame as signed 16-bit PCM
  bool GetFrameS16LE(std::vector<uint8_t>* audio_data);

 private:
  // Constructor
  explicit WASAPIAudioDevice(const WASAPIAudioDeviceConfig& config);
  
  // Initialization
  bool Initialize();
  
  // Capture a raw audio frame
  bool CaptureAudioFrame();
  
  // Clean up resources
  void Cleanup();

  // Configuration
  WASAPIAudioDeviceConfig config_;
  
  // Audio format properties
  int sample_rate_;
  int channels_;
  int bits_per_sample_;
  int frame_size_;
  UINT32 buffer_frame_count_;
  
  // Windows COM objects
  IMMDeviceEnumerator* device_enumerator_;
  IMMDevice* audio_device_;
  IAudioClient* audio_client_;
  IAudioCaptureClient* capture_client_;
  
  // Raw audio buffer
  std::vector<uint8_t> raw_buffer_;
  
  // COM initialization flag
  bool com_initialized_;
};

// Helper function to convert WAVEFORMATEX to a string for debugging
std::string FormatToString(WAVEFORMATEX* format);

// Helper function to convert HRESULT to human-readable error message
std::string HResultToString(HRESULT hr);

}  // namespace media

#endif  // WASAPI_AUDIO_DEVICE_H_
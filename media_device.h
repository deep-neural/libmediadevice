#ifndef MEDIA_DEVICE_H_
#define MEDIA_DEVICE_H_

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace media {

// Video device types
enum class VideoDeviceType {
#ifdef _WIN32
  DXGI,
#else
  X11,
  NVFBC,
#endif
};

// Audio device types
enum class AudioDeviceType {
#ifdef _WIN32
  WASAPI,
#else
  PULSE,
#endif
};

// Configuration for video device
struct VideoDeviceConfig {
  VideoDeviceType type;
  bool capture_cursor = true;
  std::string display_id = "";  // Platform default if empty
  
  // Additional platform-specific options
#ifndef _WIN32
  bool use_shm = true;  // Only used by X11
#endif
};

// Configuration for audio device
struct AudioDeviceConfig {
  AudioDeviceType type;
  int sample_rate = 44100;
  int channels = 2;
  int buffer_ms = 100;
  std::string device_id = "";  // Platform default if empty
};

// Video device interface
class VideoDevice {
 public:
  // Create a video device with the specified configuration
  static std::unique_ptr<VideoDevice> Create(const VideoDeviceConfig& config);

  virtual ~VideoDevice() = default;

  // Get dimensions of the captured frame
  virtual int GetWidth() const = 0;
  virtual int GetHeight() const = 0;

  // Capture a frame in BGRA format
  // For consistent API across implementations:
  // - Returns true if successful, false otherwise
  // - Takes a pre-allocated buffer (width * height * 4 bytes)
  virtual bool GetFrameBGRA(uint8_t* bgra_data) = 0;

#ifndef _WIN32
  // NVFBC-specific formats (only available on Linux with NVIDIA GPUs)
  virtual bool GetFrameYUV420(std::vector<uint8_t>* data);
  virtual bool GetFrameNV12(std::vector<uint8_t>* data);
#endif
};

// Audio device interface
class AudioDevice {
 public:
  // Create an audio device with the specified configuration
  static std::unique_ptr<AudioDevice> Create(const AudioDeviceConfig& config);

  virtual ~AudioDevice() = default;

  // Get a frame of audio data in signed 16-bit little-endian format
  // Returns true on success, false on failure
  virtual bool GetFrameS16LE(std::vector<uint8_t>* audio_data) = 0;

  // Get the current audio configuration
  virtual AudioDeviceConfig GetConfig() const = 0;
};

}  // namespace media

#endif  // MEDIA_DEVICE_H_
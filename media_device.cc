#include "media_device.h"

// Include platform-specific headers in the implementation file only
#ifdef _WIN32
#include "dxgi_video_device.h"
#include "wasapi_audio_device.h"
#else // Linux
#include "x11_video_device.h"
#include "nvfbc_video_device.h"
#include "pulse_audio_device.h"
#endif

#include <cstring> // For memcpy

namespace media {

//
// VideoDevice implementation
//

// Implementation classes - defined in the cc file to hide platform-specific details
#ifdef _WIN32
// Windows video device implementation
class DXGIVideoDeviceImpl : public VideoDevice {
 public:
  explicit DXGIVideoDeviceImpl(std::unique_ptr<DXGIVideoDevice> device)
      : device_(std::move(device)) {}
  
  int GetWidth() const override { return device_->GetWidth(); }
  int GetHeight() const override { return device_->GetHeight(); }
  
  bool GetFrameBGRA(uint8_t* bgra_data) override {
    return device_->GetFrameBGRA(bgra_data) == 1;
  }
  
 private:
  std::unique_ptr<DXGIVideoDevice> device_;
};
#else
// Linux video device implementations
class X11VideoDeviceImpl : public VideoDevice {
 public:
  explicit X11VideoDeviceImpl(std::unique_ptr<X11VideoDevice> device)
      : device_(std::move(device)) {}
  
  int GetWidth() const override { return device_->GetWidth(); }
  int GetHeight() const override { return device_->GetHeight(); }
  
  bool GetFrameBGRA(uint8_t* bgra_data) override {
    return device_->GetFrameBGRA(bgra_data);
  }
  
 private:
  std::unique_ptr<X11VideoDevice> device_;
};

class NVFBCVideoDeviceImpl : public VideoDevice {
 public:
  explicit NVFBCVideoDeviceImpl(std::unique_ptr<NVFBCVideoDevice> device)
      : device_(std::move(device)) {}
  
  int GetWidth() const override { return device_->GetWidth(); }
  int GetHeight() const override { return device_->GetHeight(); }
  
  bool GetFrameBGRA(uint8_t* bgra_data) override {
    std::vector<uint8_t> buffer;
    bool success = device_->GetFrameBGRA(&buffer);
    if (success && !buffer.empty()) {
      std::memcpy(bgra_data, buffer.data(), buffer.size());
    }
    return success;
  }
  
  bool GetFrameNV12(std::vector<uint8_t>* data) override {
    return device_->GetFrameNV12(data);
  }
  
 private:
  std::unique_ptr<NVFBCVideoDevice> device_;
};
#endif

std::unique_ptr<VideoDevice> VideoDevice::Create(const VideoDeviceConfig& config) {
#ifdef _WIN32
  // Windows implementation
  if (config.type == VideoDeviceType::DXGI) {
    DXGIVideoDeviceConfig dxgi_config;
    dxgi_config.cursor = config.capture_cursor;
    dxgi_config.display_id = config.display_id;
    
    auto dxgi_device = DXGIVideoDevice::Create(dxgi_config);
    if (dxgi_device) {
      return std::make_unique<DXGIVideoDeviceImpl>(std::move(dxgi_device));
    }
  }
#else
  // Linux implementation
  if (config.type == VideoDeviceType::X11) {
    X11VideoDeviceConfig x11_config;
    x11_config.cursor = config.capture_cursor;
    x11_config.display_id = config.display_id;
    x11_config.use_shm = config.use_shm;
    
    auto x11_device = X11VideoDevice::Create(x11_config);
    if (x11_device) {
      return std::make_unique<X11VideoDeviceImpl>(std::move(x11_device));
    }
  } else if (config.type == VideoDeviceType::NVFBC) {
    NVFBCVideoDeviceConfig nvfbc_config;
    nvfbc_config.cursor = config.capture_cursor;
    nvfbc_config.display_id = config.display_id;
    
    auto nvfbc_device = NVFBCVideoDevice::Create(nvfbc_config);
    if (nvfbc_device) {
      return std::make_unique<NVFBCVideoDeviceImpl>(std::move(nvfbc_device));
    }
  }
#endif

  // Return nullptr if no suitable device could be created
  return nullptr;
}

#ifndef _WIN32
// Default implementations for platform-specific methods
bool VideoDevice::GetFrameYUV420(std::vector<uint8_t>* data) {
  return false;  // Not supported by default
}

bool VideoDevice::GetFrameNV12(std::vector<uint8_t>* data) {
  return false;  // Not supported by default
}
#endif

//
// AudioDevice implementation
//

// Implementation classes - defined in the cc file to hide platform-specific details
#ifdef _WIN32
// Windows audio device implementation
class WASAPIAudioDeviceImpl : public AudioDevice {
 public:
  explicit WASAPIAudioDeviceImpl(std::unique_ptr<WASAPIAudioDevice> device, 
                               const AudioDeviceConfig& config)
      : device_(std::move(device)), config_(config) {}
  
  bool GetFrameS16LE(std::vector<uint8_t>* audio_data) override {
    return device_->GetFrameS16LE(audio_data);
  }
  
  AudioDeviceConfig GetConfig() const override {
    return config_;
  }
  
 private:
  std::unique_ptr<WASAPIAudioDevice> device_;
  AudioDeviceConfig config_;
};
#else
// Linux audio device implementation
class PulseAudioDeviceImpl : public AudioDevice {
 public:
  explicit PulseAudioDeviceImpl(std::unique_ptr<PulseAudioDevice> device, 
                              const AudioDeviceConfig& config)
      : device_(std::move(device)), config_(config) {}
  
  bool GetFrameS16LE(std::vector<uint8_t>* audio_data) override {
    return device_->GetFrameS16LE(audio_data);
  }
  
  AudioDeviceConfig GetConfig() const override {
    return config_;
  }
  
 private:
  std::unique_ptr<PulseAudioDevice> device_;
  AudioDeviceConfig config_;
};
#endif

std::unique_ptr<AudioDevice> AudioDevice::Create(const AudioDeviceConfig& config) {
#ifdef _WIN32
  // Windows implementation
  if (config.type == AudioDeviceType::WASAPI) {
    WASAPIAudioDeviceConfig wasapi_config;
    wasapi_config.device_id = config.device_id;
    wasapi_config.sample_rate = config.sample_rate;
    wasapi_config.channels = config.channels;
    wasapi_config.buffer_ms = config.buffer_ms;
    
    auto wasapi_device = WASAPIAudioDevice::Create(wasapi_config);
    if (wasapi_device) {
      return std::make_unique<WASAPIAudioDeviceImpl>(
          std::move(wasapi_device), config);
    }
  }
#else
  // Linux implementation
  if (config.type == AudioDeviceType::PULSE) {
    PulseAudioDeviceConfig pulse_config;
    pulse_config.device_id = config.device_id;
    pulse_config.sample_rate = config.sample_rate;
    pulse_config.channels = config.channels;
    pulse_config.buffer_ms = config.buffer_ms;
    
    auto pulse_device = PulseAudioDevice::Create(pulse_config);
    if (pulse_device) {
      return std::make_unique<PulseAudioDeviceImpl>(
          std::move(pulse_device), config);
    }
  }
#endif

  // Return nullptr if no suitable device could be created
  return nullptr;
}

}  // namespace media
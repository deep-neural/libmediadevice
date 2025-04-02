#ifndef MEDIA_DXGI_VIDEO_DEVICE_H_
#define MEDIA_DXGI_VIDEO_DEVICE_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <string>
#include <memory>

namespace media {

struct DXGIVideoDeviceConfig {
  bool cursor;
  std::string display_id;
};

class DXGIVideoDevice {
 public:
  // Creates a new instance of DXGIVideoDevice.
  // Returns nullptr if initialization failed.
  static std::unique_ptr<DXGIVideoDevice> Create(const DXGIVideoDeviceConfig& config);

  ~DXGIVideoDevice();

  // Get the width of the captured display.
  int GetWidth() const;

  // Get the height of the captured display.
  int GetHeight() const;

  // Captures the next frame and fills the provided buffer with BGRA format data.
  // Returns 1 if successful, 0 otherwise.
  // Buffer must be pre-allocated with sufficient size (width * height * 4 bytes).
  int GetFrameBGRA(uint8_t* bgra_data);

 private:
  DXGIVideoDevice();

  // Initializes the DXGI device and duplication interface.
  bool Initialize(const DXGIVideoDeviceConfig& config);

  // Find the specified output based on display_id.
  bool FindOutput(const std::string& display_id, IDXGIOutput** output);

  // Clean up resources
  void CleanUp();

  ID3D11Device* d3d_device_;
  ID3D11DeviceContext* d3d_context_;
  IDXGIOutputDuplication* duplication_;
  ID3D11Texture2D* staging_texture_;

  int width_;
  int height_;
  DXGI_OUTPUT_DESC output_desc_;
  bool include_cursor_;
};

}  // namespace media

#endif  // MEDIA_DXGI_VIDEO_DEVICE_H_
#include "dxgi_video_device.h"

#include <windows.h>
#include <algorithm>
#include <iostream>

namespace media {

std::unique_ptr<DXGIVideoDevice> DXGIVideoDevice::Create(const DXGIVideoDeviceConfig& config) {
  std::unique_ptr<DXGIVideoDevice> device(new DXGIVideoDevice());
  if (!device->Initialize(config)) {
    return nullptr;
  }
  return device;
}

DXGIVideoDevice::DXGIVideoDevice()
    : d3d_device_(nullptr),
      d3d_context_(nullptr),
      duplication_(nullptr),
      staging_texture_(nullptr),
      width_(0),
      height_(0),
      include_cursor_(false) {
  ZeroMemory(&output_desc_, sizeof(output_desc_));
}

DXGIVideoDevice::~DXGIVideoDevice() {
  CleanUp();
}

void DXGIVideoDevice::CleanUp() {
  if (duplication_) {
    duplication_->ReleaseFrame();
    duplication_->Release();
    duplication_ = nullptr;
  }

  if (staging_texture_) {
    staging_texture_->Release();
    staging_texture_ = nullptr;
  }

  if (d3d_context_) {
    d3d_context_->Release();
    d3d_context_ = nullptr;
  }

  if (d3d_device_) {
    d3d_device_->Release();
    d3d_device_ = nullptr;
  }
}

bool DXGIVideoDevice::Initialize(const DXGIVideoDeviceConfig& config) {
  include_cursor_ = config.cursor;

  // Create D3D11 device
  D3D_FEATURE_LEVEL feature_levels[] = {
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_9_2,
    D3D_FEATURE_LEVEL_9_1
  };
  D3D_FEATURE_LEVEL feature_level;

  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                 flags, feature_levels,
                                 ARRAYSIZE(feature_levels), D3D11_SDK_VERSION,
                                 &d3d_device_, &feature_level, &d3d_context_);
  if (FAILED(hr)) {
    return false;
  }

  // Get DXGI device
  IDXGIDevice* dxgi_device = nullptr;
  hr = d3d_device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi_device));
  if (FAILED(hr)) {
    CleanUp();
    return false;
  }

  // Get DXGI adapter
  IDXGIAdapter* dxgi_adapter = nullptr;
  hr = dxgi_device->GetAdapter(&dxgi_adapter);
  dxgi_device->Release();
  if (FAILED(hr)) {
    CleanUp();
    return false;
  }

  // Find the requested output
  IDXGIOutput* dxgi_output = nullptr;
  bool found = FindOutput(config.display_id, &dxgi_output);
  if (!found) {
    dxgi_adapter->Release();
    CleanUp();
    return false;
  }
  dxgi_adapter->Release();

  // Get output description to retrieve dimensions
  hr = dxgi_output->GetDesc(&output_desc_);
  if (FAILED(hr)) {
    dxgi_output->Release();
    CleanUp();
    return false;
  }

  width_ = output_desc_.DesktopCoordinates.right - output_desc_.DesktopCoordinates.left;
  height_ = output_desc_.DesktopCoordinates.bottom - output_desc_.DesktopCoordinates.top;

  // QI for IDXGIOutput1
  IDXGIOutput1* dxgi_output1 = nullptr;
  hr = dxgi_output->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&dxgi_output1));
  dxgi_output->Release();
  if (FAILED(hr)) {
    CleanUp();
    return false;
  }

  // Create desktop duplication
  hr = dxgi_output1->DuplicateOutput(d3d_device_, &duplication_);
  dxgi_output1->Release();
  if (FAILED(hr)) {
    CleanUp();
    return false;
  }

  // Create staging texture for CPU access
  D3D11_TEXTURE2D_DESC texture_desc = {};
  texture_desc.Width = width_;
  texture_desc.Height = height_;
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = 1;
  texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Usage = D3D11_USAGE_STAGING;
  texture_desc.BindFlags = 0;
  texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  
  hr = d3d_device_->CreateTexture2D(&texture_desc, nullptr, &staging_texture_);
  if (FAILED(hr)) {
    CleanUp();
    return false;
  }

  return true;
}

bool DXGIVideoDevice::FindOutput(const std::string& display_id, IDXGIOutput** output) {
  IDXGIDevice* dxgi_device = nullptr;
  HRESULT hr = d3d_device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi_device));
  if (FAILED(hr)) {
    return false;
  }

  IDXGIAdapter* dxgi_adapter = nullptr;
  hr = dxgi_device->GetAdapter(&dxgi_adapter);
  dxgi_device->Release();
  if (FAILED(hr)) {
    return false;
  }

  // If display_id is empty or default, use the first output
  if (display_id.empty() || display_id == "\\\\.\\DISPLAY1") {
    hr = dxgi_adapter->EnumOutputs(0, output);
    dxgi_adapter->Release();
    return SUCCEEDED(hr);
  }

  // Otherwise, iterate through outputs to find the matching one
  UINT i = 0;
  IDXGIOutput* current_output = nullptr;
  bool found = false;
  
  while (dxgi_adapter->EnumOutputs(i, &current_output) != DXGI_ERROR_NOT_FOUND) {
    DXGI_OUTPUT_DESC desc;
    hr = current_output->GetDesc(&desc);
    
    if (SUCCEEDED(hr)) {
      char device_name[256];
      WideCharToMultiByte(CP_ACP, 0, desc.DeviceName, -1, 
                          device_name, sizeof(device_name), NULL, NULL);
      
      if (display_id == device_name) {
        *output = current_output;
        found = true;
        break;
      }
    }
    
    current_output->Release();
    current_output = nullptr;
    i++;
  }

  dxgi_adapter->Release();
  return found;
}

int DXGIVideoDevice::GetWidth() const {
  return width_;
}

int DXGIVideoDevice::GetHeight() const {
  return height_;
}

int DXGIVideoDevice::GetFrameBGRA(uint8_t* bgra_data) {
  if (!duplication_ || !staging_texture_ || !bgra_data) {
    return 0;
  }

  DXGI_OUTDUPL_FRAME_INFO frame_info;
  IDXGIResource* desktop_resource = nullptr;
  HRESULT hr = duplication_->AcquireNextFrame(100, &frame_info, &desktop_resource);
  
  if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
    return 0;  // No new frame available
  }
  
  if (FAILED(hr)) {
    return 0;
  }

  // Get the desktop texture
  ID3D11Texture2D* desktop_texture = nullptr;
  hr = desktop_resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&desktop_texture));
  desktop_resource->Release();
  if (FAILED(hr)) {
    duplication_->ReleaseFrame();
    return 0;
  }

  // Copy to staging texture
  d3d_context_->CopyResource(staging_texture_, desktop_texture);
  desktop_texture->Release();

  // Map staging texture
  D3D11_MAPPED_SUBRESOURCE mapped_resource;
  hr = d3d_context_->Map(staging_texture_, 0, D3D11_MAP_READ, 0, &mapped_resource);
  if (FAILED(hr)) {
    duplication_->ReleaseFrame();
    return 0;
  }

  // Copy data row by row (handle stride)
  const uint8_t* src = static_cast<const uint8_t*>(mapped_resource.pData);
  uint8_t* dst = bgra_data;
  
  for (int y = 0; y < height_; ++y) {
    memcpy(dst, src, width_ * 4);
    src += mapped_resource.RowPitch;
    dst += width_ * 4;
  }

  // Unmap texture
  d3d_context_->Unmap(staging_texture_, 0);

  // Process cursor if needed
  if (include_cursor_ && frame_info.LastMouseUpdateTime.QuadPart != 0) {
    // Cursor processing would be implemented here
    // This would involve getting cursor information and blending it into the frame
    // Not implemented in this basic version
  }

  // Release frame
  duplication_->ReleaseFrame();
  
  return 1;
}

}  // namespace media
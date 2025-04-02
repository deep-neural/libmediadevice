#include "x11_video_device.h"

#include <iostream>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/shm.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <cerrno>
#include <cstring>

namespace media {

std::unique_ptr<X11VideoDevice> X11VideoDevice::Create(const X11VideoDeviceConfig& config) {
  std::unique_ptr<X11VideoDevice> device(new X11VideoDevice(config));
  if (!device->Initialize()) {
    return nullptr;
  }
  return device;
}

X11VideoDevice::X11VideoDevice(const X11VideoDeviceConfig& config)
    : config_(config) {
}

X11VideoDevice::~X11VideoDevice() {
  // Clean up shared memory resources
  CleanupShm();
  
  // Disconnect from X server
  if (connection_) {
    xcb_disconnect(connection_);
    connection_ = nullptr;
  }
}

bool X11VideoDevice::Initialize() {
  // Open the XCB connection
  const char* display_name = config_.display_id.empty() ? nullptr : config_.display_id.c_str();
  int screen_num;
  
  connection_ = xcb_connect(display_name, &screen_num);
  if (xcb_connection_has_error(connection_)) {
    std::cerr << "Failed to connect to X server: " << config_.display_id << std::endl;
    return false;
  }

  // Get screen information
  const xcb_setup_t* setup = xcb_get_setup(connection_);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  
  // Advance to the requested screen
  for (int i = 0; i < screen_num; ++i) {
    xcb_screen_next(&iter);
  }
  
  if (iter.rem == 0) {
    std::cerr << "Failed to find the requested screen" << std::endl;
    xcb_disconnect(connection_);
    connection_ = nullptr;
    return false;
  }
  
  screen_ = iter.data;
  root_window_ = screen_->root;
  
  // Store dimensions
  width_ = screen_->width_in_pixels;
  height_ = screen_->height_in_pixels;
  
  // Check if SHM is available and initialize it if requested
  if (config_.use_shm) {
    // Query for SHM extension
    const xcb_query_extension_reply_t* ext_reply = 
        xcb_get_extension_data(connection_, &xcb_shm_id);
    
    if (ext_reply && ext_reply->present) {
      // SHM is available, check version
      xcb_shm_query_version_cookie_t ver_cookie = 
          xcb_shm_query_version(connection_);
      xcb_shm_query_version_reply_t* ver_reply = 
          xcb_shm_query_version_reply(connection_, ver_cookie, nullptr);
      
      if (ver_reply) {
        // SHM extension is present, initialize shared memory
        has_shm_ = InitializeShm();
        free(ver_reply);
      }
    }
    
    if (!has_shm_) {
      std::cerr << "XCB SHM extension not available, falling back to standard mode" << std::endl;
    }
  }
  
  return true;
}

bool X11VideoDevice::InitializeShm() {
  // Calculate the size needed for the image (BGRA - 4 bytes per pixel)
  shm_size_ = width_ * height_ * 4;
  
  // Create a shared memory segment
  shm_id_ = shmget(IPC_PRIVATE, shm_size_, IPC_CREAT | 0777);
  if (shm_id_ == -1) {
    std::cerr << "Failed to create shared memory segment: " << strerror(errno) << std::endl;
    return false;
  }
  
  // Attach the segment to our process
  shm_addr_ = shmat(shm_id_, nullptr, 0);
  if (shm_addr_ == reinterpret_cast<void*>(-1)) {
    std::cerr << "Failed to attach shared memory: " << strerror(errno) << std::endl;
    shmctl(shm_id_, IPC_RMID, nullptr);
    shm_id_ = -1;
    return false;
  }
  
  // Attach the segment to X server
  shm_seg_ = xcb_generate_id(connection_);
  xcb_void_cookie_t attach_cookie = xcb_shm_attach_checked(connection_, shm_seg_, shm_id_, false);
  
  xcb_generic_error_t* error = xcb_request_check(connection_, attach_cookie);
  
  if (error) {
    std::cerr << "Failed to attach shared memory to X server: error code " 
              << error->error_code << std::endl;
    free(error);
    shmdt(shm_addr_);
    shmctl(shm_id_, IPC_RMID, nullptr);
    shm_addr_ = nullptr;
    shm_id_ = -1;
    return false;
  }
  
  // Mark the segment for deletion after detach
  shmctl(shm_id_, IPC_RMID, nullptr);
  
  return true;
}

void X11VideoDevice::CleanupShm() {
  if (has_shm_) {
    if (connection_ && shm_seg_) {
      xcb_shm_detach(connection_, shm_seg_);
      shm_seg_ = 0;
    }
    
    if (shm_addr_ != nullptr) {
      shmdt(shm_addr_);
      shm_addr_ = nullptr;
    }
    
    has_shm_ = false;
  }
}

int X11VideoDevice::GetWidth() const {
  return width_;
}

int X11VideoDevice::GetHeight() const {
  return height_;
}

bool X11VideoDevice::GetFrameBGRA(uint8_t* bgra_data) {
  if (!connection_ || !screen_ || !bgra_data) {
    return false;
  }
  
  // Use shared memory if available, otherwise fall back to standard method
  if (has_shm_ && shm_addr_) {
    return GetFrameShm(bgra_data);
  } else {
    return GetFrameStandard(bgra_data);
  }
}

bool X11VideoDevice::GetFrameStandard(uint8_t* bgra_data) {
  // Get the image from the root window (full screen)
  xcb_get_image_cookie_t cookie = xcb_get_image(
      connection_,
      XCB_IMAGE_FORMAT_Z_PIXMAP,
      root_window_,
      0, 0,                // x, y
      width_, height_,     // width, height
      ~0                   // plane mask (all planes)
  );
  
  xcb_generic_error_t* error = nullptr;
  xcb_get_image_reply_t* reply = xcb_get_image_reply(connection_, cookie, &error);
  
  if (error) {
    std::cerr << "Failed to get image: error code " << error->error_code << std::endl;
    free(error);
    return false;
  }
  
  if (!reply) {
    std::cerr << "Failed to get image: null reply" << std::endl;
    return false;
  }
  
  // Get the image data
  uint8_t* src_data = xcb_get_image_data(reply);
  uint32_t src_data_len = xcb_get_image_data_length(reply);
  
  // Copy data to the output buffer in BGRA format
  // Note: Depending on the X server's pixel format, you might need a more complex conversion here
  std::memcpy(bgra_data, src_data, src_data_len);
  
  free(reply);
  return true;
}

bool X11VideoDevice::GetFrameShm(uint8_t* bgra_data) {
  // Get the image using shared memory
  xcb_shm_get_image_cookie_t cookie = xcb_shm_get_image(
      connection_,
      root_window_,
      0, 0,               // x, y
      width_, height_,    // width, height
      ~0,                 // plane mask (all planes)
      XCB_IMAGE_FORMAT_Z_PIXMAP,  // format
      shm_seg_,           // shared memory segment
      0                   // offset within segment
  );
  
  xcb_generic_error_t* error = nullptr;
  xcb_shm_get_image_reply_t* reply = 
      xcb_shm_get_image_reply(connection_, cookie, &error);
  
  if (error) {
    std::cerr << "Failed to get image with SHM: error code " 
              << error->error_code << std::endl;
    free(error);
    return false;
  }
  
  if (!reply) {
    std::cerr << "Failed to get image with SHM: null reply" << std::endl;
    return false;
  }
  
  // Copy data from shared memory to the output buffer
  std::memcpy(bgra_data, shm_addr_, shm_size_);
  
  free(reply);
  return true;
}

}  // namespace media
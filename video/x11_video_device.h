#ifndef MEDIA_X11_VIDEO_DEVICE_H_
#define MEDIA_X11_VIDEO_DEVICE_H_

#include <memory>
#include <string>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/shm.h>
#include <sys/shm.h>

namespace media {

struct X11VideoDeviceConfig {
  bool cursor = false;
  std::string display_id = ":0";
  bool use_shm = true;  // Option to use shared memory (default: true)
};

class X11VideoDevice {
 public:
  // Factory method to create the X11VideoDevice
  static std::unique_ptr<X11VideoDevice> Create(const X11VideoDeviceConfig& config);
  
  // Destructor
  ~X11VideoDevice();

  // Gets the width of the display
  int GetWidth() const;
  
  // Gets the height of the display
  int GetHeight() const;
  
  // Captures a frame in BGRA format
  // Returns true if successful, false otherwise
  bool GetFrameBGRA(uint8_t* bgra_data);

 private:
  // Private constructor - only accessible via Create factory method
  X11VideoDevice(const X11VideoDeviceConfig& config);
  
  // Initializes the XCB connection and screen
  bool Initialize();
  
  // Initialize shared memory segment
  bool InitializeShm();
  
  // Clean up shared memory resources
  void CleanupShm();
  
  // Get frame using regular (non-SHM) method
  bool GetFrameStandard(uint8_t* bgra_data);
  
  // Get frame using shared memory method
  bool GetFrameShm(uint8_t* bgra_data);
  
  // Member variables
  X11VideoDeviceConfig config_;
  xcb_connection_t* connection_ = nullptr;
  xcb_screen_t* screen_ = nullptr;
  xcb_window_t root_window_ = 0;
  
  int width_ = 0;
  int height_ = 0;
  
  // Shared memory related members
  bool has_shm_ = false;
  xcb_shm_seg_t shm_seg_ = 0;
  int shm_id_ = -1;
  void* shm_addr_ = nullptr;
  size_t shm_size_ = 0;
  
  // Prevent copy and assignment
  X11VideoDevice(const X11VideoDevice&) = delete;
  X11VideoDevice& operator=(const X11VideoDevice&) = delete;
};

}  // namespace media

#endif  // MEDIA_X11_VIDEO_DEVICE_H_
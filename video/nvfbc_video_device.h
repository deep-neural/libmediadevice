#ifndef NVFBC_VIDEO_DEVICE_H_
#define NVFBC_VIDEO_DEVICE_H_

#include <memory>
#include <string>
#include <vector>
#include "nvfbc/nvfbc.h"

namespace media {

/**
 * Configuration options for the NVFBC video device
 */
struct NVFBCVideoDeviceConfig {
    bool cursor = true;           // Include cursor in captures
    std::string display_id = "";  // X11 display identifier (e.g., ":0", ":1")
                                  // Empty string means default display
};

/**
 * Class for capturing screen content using NVIDIA's Frame Buffer Capture (NVFBC) API
 */
class NVFBCVideoDevice {
public:
    /**
     * Creates a new NVFBC video device instance
     * 
     * @param config Configuration options for the device
     * @return Unique pointer to a new device or nullptr if creation failed
     */
    static std::unique_ptr<NVFBCVideoDevice> Create(const NVFBCVideoDeviceConfig& config);
    
    /**
     * Destructor
     */
    virtual ~NVFBCVideoDevice() = default;
    
    /**
     * Get the width of the captured frame
     * 
     * @return Width in pixels
     */
    virtual int GetWidth() const = 0;
    
    /**
     * Get the height of the captured frame
     * 
     * @return Height in pixels
     */
    virtual int GetHeight() const = 0;
    
    virtual bool GetFrameARGB(std::vector<uint8_t>* data) = 0;

    virtual bool GetFrameRGBA(std::vector<uint8_t>* data) = 0;
    
    virtual bool GetFrameBGRA(std::vector<uint8_t>* data) = 0;
    
    virtual bool GetFrameRGB(std::vector<uint8_t>* data) = 0;
    
    virtual bool GetFrameNV12(std::vector<uint8_t>* data) = 0;
    
    virtual bool GetFrameYUV444P(std::vector<uint8_t>* data) = 0;
};

} // namespace media

#endif // NVFBC_VIDEO_DEVICE_H_
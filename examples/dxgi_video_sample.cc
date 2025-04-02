// dxgi_video_sample.cc
#include "media_device.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#ifdef _WIN32

int main() {
    // Create video device configuration for DXGI
    media::VideoDeviceConfig config;
    config.type = media::VideoDeviceType::DXGI;
    config.capture_cursor = true;
    config.display_id = "";  // Use default display
    
    // Create the video device
    auto video_device = media::VideoDevice::Create(config);
    if (!video_device) {
        std::cerr << "Failed to create DXGI video device!" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully created DXGI video device" << std::endl;
    std::cout << "Capture resolution: " << video_device->GetWidth() << "x" << video_device->GetHeight() << std::endl;
    
    // Allocate buffer for frame data
    int buffer_size = video_device->GetWidth() * video_device->GetHeight() * 4;  // BGRA = 4 bytes per pixel
    std::vector<uint8_t> frame_buffer(buffer_size);
    
    // Capture 100 frames
    for (int i = 0; i < 100; ++i) {
        if (video_device->GetFrameBGRA(frame_buffer.data())) {
            std::cout << "Successfully captured frame " << (i + 1) << std::endl;
        } else {
            std::cerr << "Failed to capture frame " << (i + 1) << std::endl;
        }
        
        // Sleep for a short time between frames
        std::this_thread::sleep_for(std::chrono::milliseconds(33));  // ~30 fps
    }
    
    std::cout << "Captured 100 frames. Exiting." << std::endl;
    return 0;
}

#else

int main() {
    std::cerr << "DXGI video device is only available on Windows platforms." << std::endl;
    return 1;
}

#endif
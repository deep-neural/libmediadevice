



// apt-get update
// apt-get install xorg dbus-x11 openbox
// sudo nvidia-xconfig --mode=1280x720

// startx -- :0

// export DISPLAY=:0
// openbox


// export DISPLAY=:0
// xrandr --newmode "1280x720_60.00" 74.50 1280 1344 1472 1664 720 723 728 748 -hsync +vsync
// xrandr --addmode DVI-D-0 "1280x720_60.00"
// xrandr --output DVI-D-0 --mode "1280x720_60.00"

// xrandr --listmonitors

// xdpyinfo | grep dimensions | awk '{print $2}'


// export DISPLAY=:0
// apt-get install mesa-utils
// glxgears



// nvfbc_video_sample.cc
#include "media_device.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#ifndef _WIN32  // Linux only

int main() {
    
    // Create video device configuration for NVFBC
    media::VideoDeviceConfig config;
    config.type = media::VideoDeviceType::NVFBC;
    config.capture_cursor = true;
    config.display_id = ":0";  // Use default display
    
    // Create the video device
    auto video_device = media::VideoDevice::Create(config);
    if (!video_device) {
        std::cerr << "Failed to create NVFBC video device!" << std::endl;
        std::cerr << "Note: NVFBC requires NVIDIA GPU and drivers" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully created NVFBC video device" << std::endl;
    std::cout << "Capture resolution: " << video_device->GetWidth() << "x" << video_device->GetHeight() << std::endl;
    
    // Test different capture formats
    int bgra_buffer_size = video_device->GetWidth() * video_device->GetHeight() * 4;  // BGRA = 4 bytes per pixel
    std::vector<uint8_t> bgra_buffer(bgra_buffer_size);
    std::vector<uint8_t> yuv420_buffer;
    std::vector<uint8_t> nv12_buffer;
    
    // Capture 100 frames in different formats
    for (int i = 0; i < 100; ++i) {
        bool bgra_success = false;
        bool yuv420_success = false;
        bool nv12_success = false;

        // Capture NV12 format
        nv12_success = video_device->GetFrameNV12(&nv12_buffer);
        if (nv12_success) {
            std::cout << "Successfully captured NV12 frame " << (i + 1)
                      << " (" << nv12_buffer.size() << " bytes)" << std::endl;
        } else {
            std::cerr << "NV12 format not supported or capture failed for frame " << (i + 1) << std::endl;
        }
        
        // Sleep for a short time between frames
        //std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 fps
    }
    
    std::cout << "Captured 100 frames. Exiting." << std::endl;
    return 0;
}

#else

int main() {
    std::cerr << "NVFBC video device is only available on Linux platforms with NVIDIA GPUs." << std::endl;
    return 1;
}

#endif
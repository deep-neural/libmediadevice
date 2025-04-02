


// apt install pulseaudio

// pulseaudio --system


// pulse_audio_sample.cc
#include "media_device.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#ifndef _WIN32  // Linux only

int main() {
    // Create audio device configuration for PulseAudio
    media::AudioDeviceConfig config;
    config.type = media::AudioDeviceType::PULSE;
    config.sample_rate = 44100;
    config.channels = 2;
    config.buffer_ms = 100;
    config.device_id = "";  // Use default audio device
    
    // Create the audio device
    auto audio_device = media::AudioDevice::Create(config);
    if (!audio_device) {
        std::cerr << "Failed to create PulseAudio device!" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully created PulseAudio device" << std::endl;
    
    // Get the actual configuration used
    auto actual_config = audio_device->GetConfig();
    std::cout << "Audio configuration:" << std::endl;
    std::cout << "  Sample rate: " << actual_config.sample_rate << " Hz" << std::endl;
    std::cout << "  Channels: " << actual_config.channels << std::endl;
    std::cout << "  Buffer size: " << actual_config.buffer_ms << " ms" << std::endl;
    
    // Calculate expected buffer size (in bytes)
    // 1 sample = 2 bytes (S16LE format)
    int bytes_per_frame = actual_config.channels * 2;
    int expected_bytes = (actual_config.sample_rate * bytes_per_frame * actual_config.buffer_ms) / 1000;
    
    // Capture 100 audio frames
    std::vector<uint8_t> audio_buffer;
    for (int i = 0; i < 100; ++i) {
        if (audio_device->GetFrameS16LE(&audio_buffer)) {
            std::cout << "Successfully captured audio frame " << (i + 1) 
                      << " (" << audio_buffer.size() << " bytes)" << std::endl;
        } else {
            std::cerr << "Failed to capture audio frame " << (i + 1) << std::endl;
        }
        
        // Sleep for the buffer duration
        std::this_thread::sleep_for(std::chrono::milliseconds(actual_config.buffer_ms));
    }
    
    std::cout << "Captured 100 audio frames. Exiting." << std::endl;
    return 0;
}

#else

int main() {
    std::cerr << "PulseAudio device is only available on Linux platforms." << std::endl;
    return 1;
}

#endif
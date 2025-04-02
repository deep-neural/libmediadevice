

Install Packages For Linux 
```bash
$ apt-get update
$ apt-get -y install libx11-dev libxcb1-dev libpulse-dev libxcb-image0-dev libxcb-shm0-dev
```


Build On Windows
```bash
$ mkdir build && cd build
$ cmake -G "Visual Studio 17 2022" .. 
$ cmake --build .
```


Build On Linux
```bash
$ mkdir build && cd build
$ cmake .. 
$ cmake --build .
```






Usage

media::WASAPIAudioDeviceConfig config {
    .sample_rate = 48000,
    .channels = 2,
    .buffer_ms = 10,
    .device_id = "",
};

auto audio_device = media::WASAPIAudioDevice::Create(config);
if(!audio_device) {

}

std::vector<uint8_t> audio_data_pcm_S16LE;
int success = audio_device->GetFrameS16LE(&audio_data);
if(!success) {

}


create the files below like above format, using windows to capture audio whole system 

wasapi_audio_device.cc
wasapi_audio_device.h





media::WASAPIAudioDeviceConfig config {
    .sample_rate = 48000,
    .channels = 2,
    .buffer_ms = 10,
    .device_id = "",  // Empty string for default device
    .loopback = true  // Set to true for capturing system audio output
};
auto audio_device = media::WASAPIAudioDevice::Create(config);
if(!audio_device) {
    // Handle error
}
std::vector<uint8_t> audio_data_pcm_S16LE;
int success = audio_device->GetFrameS16LE(&audio_data_pcm_S16LE);
if(!success) {
    // Handle error
}




media::DXGIVideoDeviceConfig config {
	.width = 1920,
	.height = 1080,
	.framerate = 60,
        .cursor = false,
        display_id = "\\\\.\\DISPLAY1",
};

auto display = media::DXGIVideoDevice::Create(config);
if(!encoder) { 

}




media::NVFBCVideoDeviceConfig config {
    .cursor = false,
    display_id = ":0",
};

auto display = media::NVFBCVideoDevice::Create(config);
if(!display) { 

}

int width = display->GetWidth();

int height = display->GetHeight();

std::vector<uint8_t> bgra_data;

int success = display->GetFrameBGRA(&bgra_data);
if(!success) {

}

std::vector<uint8_t> YUV420_data;

int success = display->GetFrameYUV420(&YUV420_data);
if(!success) {

}

std::vector<uint8_t> NV12_data;

int success = display->GetFrameNV12(&NV12_data);
if(!success) {

}

create the files below like above format, using only the nvidia file i gave you to capture the screen buffers
use X11Display to set the the current display of the process to display_id

nvfb_video_device.cc
nvfb_video_device.h





media::NVFBCVideoDeviceConfig config {
	.width = 1920,
	.height = 1080,
	.framerate = 60,
        .cursor = false,
        display_id = ":0",
};

auto display = media::NVFBCVideoDevice::Create(config);
if(!encoder) { 

}



std::vector<uint8_t> yuv_data;

int success = display->GetFrameYUV420(&yuv_data);
if(!success) {

}


int success = display->GetFrameNV12(&yuv_data);
if(!success) {

}

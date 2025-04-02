#include "nvfbc_video_device.h"
#include <dlfcn.h>
#include <cstring>
#include <iostream>
#include <X11/Xlib.h>

namespace media {

// NVFBC Library Name
#define LIB_NVFBC_NAME "libnvidia-fbc.so.1"

namespace {

// Private implementation of the NVFBCVideoDevice interface
class NVFBCVideoDeviceImpl : public NVFBCVideoDevice {
public:
    explicit NVFBCVideoDeviceImpl(const NVFBCVideoDeviceConfig& config);
    ~NVFBCVideoDeviceImpl() override;

    int GetWidth() const override;
    int GetHeight() const override;

    bool GetFrameARGB(std::vector<uint8_t>* data) override;
    bool GetFrameRGBA(std::vector<uint8_t>* data) override;
    bool GetFrameBGRA(std::vector<uint8_t>* data) override;
    bool GetFrameRGB(std::vector<uint8_t>* data) override;
    bool GetFrameNV12(std::vector<uint8_t>* data) override;
    bool GetFrameYUV444P(std::vector<uint8_t>* data) override;

private:
    // Initialize X11 display and get resolution
    bool InitializeX11Display();
    
    // Initialize NVFBC library
    bool InitializeNvFBC();
    
    // Create and setup capture session
    bool CreateCaptureSession();
    
    // Grab a frame with specified format
    bool GrabFrame(NVFBC_BUFFER_FORMAT format, std::vector<uint8_t>* data);
    
    // Calculate frame size based on format
    size_t CalculateFrameSize(NVFBC_BUFFER_FORMAT format) const;
    
    // Cleanup functions
    void DestroyCaptureSession();
    void DestroyHandle();
    void CloseX11Display();

    // NVFBC related members
    NVFBC_SESSION_HANDLE m_session = 0;
    void* m_libNVFBC = nullptr;
    NVFBC_API_FUNCTION_LIST* m_pFn = nullptr;
    void (*NvFBCCreateInstance_ptr)(NVFBC_API_FUNCTION_LIST*) = nullptr;
    
    // X11 display related members
    Display* m_display = nullptr;
    int m_screen = 0;
    
    // Config settings
    NVFBCVideoDeviceConfig m_config;
    
    // Display dimensions
    int m_width = 0;
    int m_height = 0;
};

NVFBCVideoDeviceImpl::NVFBCVideoDeviceImpl(const NVFBCVideoDeviceConfig& config)
    : m_config(config) {
    if (!InitializeX11Display()) {
        std::cerr << "X11 display initialization failed." << std::endl;
        return;
    }

    if (!InitializeNvFBC()) {
        std::cerr << "NVFBC Initialization failed." << std::endl;
        CloseX11Display();
        return;
    }

    if (!CreateCaptureSession()) {
        std::cerr << "NVFBC Create Capture Session failed." << std::endl;
        CloseX11Display();
        return;
    }
}

NVFBCVideoDeviceImpl::~NVFBCVideoDeviceImpl() {
    DestroyCaptureSession();
    DestroyHandle();

    if (m_libNVFBC) {
        dlclose(m_libNVFBC);
        m_libNVFBC = nullptr;
    }

    CloseX11Display();
}

bool NVFBCVideoDeviceImpl::InitializeX11Display() {

    // Open X For entire process
    if (m_config.display_id.empty()) {
        setenv("DISPLAY", ":0", 1);  // Use a default display like ":0"
    } else {
        setenv("DISPLAY", m_config.display_id.c_str(), 1);
    }

    // Open X display
    m_display = XOpenDisplay(m_config.display_id.empty() ? nullptr : m_config.display_id.c_str());
    if (!m_display) {
        std::cerr << "Failed to open X display: " << (m_config.display_id.empty() ? ":0" : m_config.display_id) << std::endl;
        return false;
    }

    // Get default screen
    m_screen = DefaultScreen(m_display);
    
    // Get screen dimensions
    m_width = DisplayWidth(m_display, m_screen);
    m_height = DisplayHeight(m_display, m_screen);
    
    std::cout << "Initialized X display with resolution: " << m_width << "x" << m_height << std::endl;
    return true;
}

void NVFBCVideoDeviceImpl::CloseX11Display() {
    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
}

int NVFBCVideoDeviceImpl::GetWidth() const {
    return m_width;
}

int NVFBCVideoDeviceImpl::GetHeight() const {
    return m_height;
}

bool NVFBCVideoDeviceImpl::GetFrameARGB(std::vector<uint8_t>* data) {
    return GrabFrame(NVFBC_BUFFER_FORMAT_ARGB, data);
}

bool NVFBCVideoDeviceImpl::GetFrameRGBA(std::vector<uint8_t>* data) {
    return GrabFrame(NVFBC_BUFFER_FORMAT_RGBA, data);
}

bool NVFBCVideoDeviceImpl::GetFrameBGRA(std::vector<uint8_t>* data) {
    return GrabFrame(NVFBC_BUFFER_FORMAT_BGRA, data);
}

bool NVFBCVideoDeviceImpl::GetFrameRGB(std::vector<uint8_t>* data) {
    return GrabFrame(NVFBC_BUFFER_FORMAT_RGB, data);
}

bool NVFBCVideoDeviceImpl::GetFrameNV12(std::vector<uint8_t>* data) {
    return GrabFrame(NVFBC_BUFFER_FORMAT_NV12, data);
}

bool NVFBCVideoDeviceImpl::GetFrameYUV444P(std::vector<uint8_t>* data) {
    return GrabFrame(NVFBC_BUFFER_FORMAT_YUV444P, data);
}

bool NVFBCVideoDeviceImpl::InitializeNvFBC() {
    m_libNVFBC = dlopen(LIB_NVFBC_NAME, RTLD_NOW);
    if (m_libNVFBC == nullptr) {
        std::cerr << "Unable to open '" << LIB_NVFBC_NAME << "': " << dlerror() << std::endl;
        return false;
    }

    NvFBCCreateInstance_ptr = reinterpret_cast<void (*)(NVFBC_API_FUNCTION_LIST*)>(
        dlsym(m_libNVFBC, "NvFBCCreateInstance"));
    if (NvFBCCreateInstance_ptr == nullptr) {
        std::cerr << "Unable to resolve symbol 'NvFBCCreateInstance': " << dlerror() << std::endl;
        return false;
    }

    m_pFn = new NVFBC_API_FUNCTION_LIST();
    memset(m_pFn, 0, sizeof(NVFBC_API_FUNCTION_LIST));
    m_pFn->dwVersion = NVFBC_VERSION;

    NVFBCSTATUS fbcStatus = ((PNVFBCCREATEINSTANCE)NvFBCCreateInstance_ptr)(m_pFn);
    if (fbcStatus != NVFBC_SUCCESS) {
        std::cerr << "Unable to create NvFBC instance (status: " << fbcStatus << ")" << std::endl;
        return false;
    }
    return true;
}

bool NVFBCVideoDeviceImpl::CreateCaptureSession() {
    NVFBCSTATUS fbcStatus;
    NVFBC_CREATE_HANDLE_PARAMS createHandleParams;
    NVFBC_CREATE_CAPTURE_SESSION_PARAMS createCaptureParams;
    NVFBC_GET_STATUS_PARAMS statusParams;

    // Create Handle
    memset(&createHandleParams, 0, sizeof(createHandleParams));
    createHandleParams.dwVersion = NVFBC_CREATE_HANDLE_PARAMS_VER;
    fbcStatus = m_pFn->nvFBCCreateHandle(&m_session, &createHandleParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        std::cerr << "NVFBC Create Handle failed: " << m_pFn->nvFBCGetLastErrorStr(m_session) << std::endl;
        return false;
    }

    // Get Status - Optional but recommended
    memset(&statusParams, 0, sizeof(statusParams));
    statusParams.dwVersion = NVFBC_GET_STATUS_PARAMS_VER;
    fbcStatus = m_pFn->nvFBCGetStatus(m_session, &statusParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        std::cerr << "NVFBC Get Status failed: " << m_pFn->nvFBCGetLastErrorStr(m_session) << std::endl;
        return false;
    }

    if (statusParams.bCanCreateNow == NVFBC_FALSE) {
        std::cerr << "Cannot create capture session on this system." << std::endl;
        return false;
    }

    // Create Capture Session
    memset(&createCaptureParams, 0, sizeof(createCaptureParams));
    createCaptureParams.dwVersion = NVFBC_CREATE_CAPTURE_SESSION_PARAMS_VER;
    createCaptureParams.eCaptureType = NVFBC_CAPTURE_TO_SYS;
    createCaptureParams.bWithCursor = m_config.cursor ? NVFBC_TRUE : NVFBC_FALSE;
    createCaptureParams.captureBox.x = 0;
    createCaptureParams.captureBox.y = 0;
    createCaptureParams.captureBox.w = m_width;
    createCaptureParams.captureBox.h = m_height;
    createCaptureParams.frameSize.w = m_width;
    createCaptureParams.frameSize.h = m_height;
    createCaptureParams.eTrackingType = NVFBC_TRACKING_SCREEN;

    fbcStatus = m_pFn->nvFBCCreateCaptureSession(m_session, &createCaptureParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        std::cerr << "NVFBC Create Capture Session failed: " << m_pFn->nvFBCGetLastErrorStr(m_session) << std::endl;
        return false;
    }
    
    return true;
}

bool NVFBCVideoDeviceImpl::GrabFrame(NVFBC_BUFFER_FORMAT format, std::vector<uint8_t>* data) {
    if (!data || m_width == 0 || m_height == 0) {
        std::cerr << "Invalid parameters for frame capture" << std::endl;
        return false;
    }

    NVFBCSTATUS fbcStatus;
    NVFBC_TOSYS_SETUP_PARAMS setupParams;
    NVFBC_TOSYS_GRAB_FRAME_PARAMS grabParams;
    NVFBC_FRAME_GRAB_INFO frameInfo;
    unsigned char* frame = nullptr;

    // Setup for the capture
    memset(&setupParams, 0, sizeof(setupParams));
    setupParams.dwVersion = NVFBC_TOSYS_SETUP_PARAMS_VER;
    setupParams.eBufferFormat = format;
    setupParams.ppBuffer = reinterpret_cast<void**>(&frame);
    setupParams.bWithDiffMap = NVFBC_FALSE;

    fbcStatus = m_pFn->nvFBCToSysSetUp(m_session, &setupParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        std::cerr << "NVFBC ToSysSetUp failed: " << m_pFn->nvFBCGetLastErrorStr(m_session) << std::endl;
        return false;
    }

    // Prepare for frame grab
    memset(&grabParams, 0, sizeof(grabParams));
    memset(&frameInfo, 0, sizeof(frameInfo));
    grabParams.dwVersion = NVFBC_TOSYS_GRAB_FRAME_PARAMS_VER;
    grabParams.dwFlags = NVFBC_TOSYS_GRAB_FLAGS_NOWAIT; // Non-blocking grab
    grabParams.pFrameGrabInfo = &frameInfo;

    // Grab the frame
    fbcStatus = m_pFn->nvFBCToSysGrabFrame(m_session, &grabParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        std::cerr << "NVFBC Grab Frame failed: " << m_pFn->nvFBCGetLastErrorStr(m_session) << std::endl;
        return false;
    }

    if (frame == nullptr) {
        std::cerr << "Frame pointer is null" << std::endl;
        return false;
    }

    // Calculate frame size and copy data
    size_t frameSize = CalculateFrameSize(format);
    data->resize(frameSize);
    std::memcpy(data->data(), frame, frameSize);

    return true;
}

size_t NVFBCVideoDeviceImpl::CalculateFrameSize(NVFBC_BUFFER_FORMAT format) const {
    size_t bytesPerPixel = 4; // Default to 4 bytes per pixel (32-bit formats)

    switch (format) {
        case NVFBC_BUFFER_FORMAT_ARGB:
        case NVFBC_BUFFER_FORMAT_RGBA:
        case NVFBC_BUFFER_FORMAT_BGRA:
            bytesPerPixel = 4;
            break;
        case NVFBC_BUFFER_FORMAT_RGB:
            bytesPerPixel = 3;
            break;
        case NVFBC_BUFFER_FORMAT_NV12:
            return m_width * m_height * 3 / 2; // Special case for NV12
        case NVFBC_BUFFER_FORMAT_YUV444P:
            return m_width * m_height * 3; // Special case for YUV444P
        default:
            std::cerr << "Unsupported format" << std::endl;
            return 0;
    }

    return m_width * m_height * bytesPerPixel;
}

void NVFBCVideoDeviceImpl::DestroyCaptureSession() {
    if (m_session) {
        NVFBCSTATUS fbcStatus;
        NVFBC_DESTROY_CAPTURE_SESSION_PARAMS destroyCaptureParams;

        memset(&destroyCaptureParams, 0, sizeof(destroyCaptureParams));
        destroyCaptureParams.dwVersion = NVFBC_DESTROY_CAPTURE_SESSION_PARAMS_VER;

        fbcStatus = m_pFn->nvFBCDestroyCaptureSession(m_session, &destroyCaptureParams);
        if (fbcStatus != NVFBC_SUCCESS) {
            std::cerr << "NVFBC Destroy Capture Session failed: " << m_pFn->nvFBCGetLastErrorStr(m_session) << std::endl;
        }
    }
}

void NVFBCVideoDeviceImpl::DestroyHandle() {
    if (m_session) {
        NVFBCSTATUS fbcStatus;
        NVFBC_DESTROY_HANDLE_PARAMS destroyHandleParams;

        memset(&destroyHandleParams, 0, sizeof(destroyHandleParams));
        destroyHandleParams.dwVersion = NVFBC_DESTROY_HANDLE_PARAMS_VER;

        fbcStatus = m_pFn->nvFBCDestroyHandle(m_session, &destroyHandleParams);
        if (fbcStatus != NVFBC_SUCCESS) {
            std::cerr << "NVFBC Destroy Handle failed: " << m_pFn->nvFBCGetLastErrorStr(m_session) << std::endl;
        }
        m_session = 0;
    }

    if (m_pFn) {
        delete m_pFn;
        m_pFn = nullptr;
    }
}

} // namespace

std::unique_ptr<NVFBCVideoDevice> NVFBCVideoDevice::Create(const NVFBCVideoDeviceConfig& config) {
    auto device = std::make_unique<NVFBCVideoDeviceImpl>(config);
    
    // Check if initialization was successful (width and height should be non-zero)
    if (device->GetWidth() == 0 || device->GetHeight() == 0) {
        return nullptr;
    }
    
    return device;
}

} // namespace media
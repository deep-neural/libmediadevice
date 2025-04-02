cmake_minimum_required(VERSION 3.10)
project(MediaDeviceExamples)

# Path to the parent directory containing libs and headers
set(BASE_DIR ${CMAKE_SOURCE_DIR})

# Include directories
include_directories(
    ${BASE_DIR}
    ${BASE_DIR}/include
)

# Print debug info
message(STATUS "Base directory: ${BASE_DIR}")
message(STATUS "Binary directory: ${CMAKE_BINARY_DIR}")

# Define platform-specific targets
if(WIN32)
    # Windows-specific examples
    add_executable(dxgi_video_sample dxgi_video_sample.cc)
    add_executable(wasapi_audio_sample wasapi_audio_sample.cc)
    
    set(EXAMPLE_TARGETS
        dxgi_video_sample
        wasapi_audio_sample
    )
    
    # Set properties for Windows targets
    foreach(TARGET ${EXAMPLE_TARGETS})
        set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 14)
        target_include_directories(${TARGET} PRIVATE ${BASE_DIR} ${BASE_DIR}/include)
        # Link directly to the mediadevice_lib target
        target_link_libraries(${TARGET} mediadevice_lib)
        # Add dependency to ensure library is built first
        add_dependencies(${TARGET} mediadevice_lib)
    endforeach()
    
elseif(UNIX AND NOT APPLE)
    # Linux-specific examples
    add_executable(x11_video_sample x11_video_sample.cc)
    add_executable(nvfbc_video_sample nvfbc_video_sample.cc)
    add_executable(pulse_audio_sample pulse_audio_sample.cc)
    
    set(EXAMPLE_TARGETS
        x11_video_sample
        nvfbc_video_sample
        pulse_audio_sample
    )
    
    # Set properties for Linux targets
    foreach(TARGET ${EXAMPLE_TARGETS})
        set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 14)
        target_include_directories(${TARGET} PRIVATE ${BASE_DIR} ${BASE_DIR}/include)
        
        # Link directly to the mediadevice_lib target
        target_link_libraries(${TARGET} mediadevice_lib)
        
        # Add dependency to ensure library is built first
        add_dependencies(${TARGET} mediadevice_lib)
        
        # Add any Linux-specific libraries if needed
        # For X11
        if(${TARGET} STREQUAL "x11_video_sample")
            find_package(X11 REQUIRED)
            target_link_libraries(${TARGET} ${X11_LIBRARIES})
        endif()
        # For PulseAudio
        if(${TARGET} STREQUAL "pulse_audio_sample")
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(PULSE REQUIRED libpulse)
            target_link_libraries(${TARGET} ${PULSE_LIBRARIES})
        endif()
        # For NVFBC - add NVIDIA FBC libraries if required
        if(${TARGET} STREQUAL "nvfbc_video_sample")
            # target_link_libraries(${TARGET} nvfbc)
        endif()
    endforeach()
    
else()
    message(WARNING "This project primarily supports Windows and Linux. Some examples may not be available on your platform.")
endif()

# Optional: Add a target that builds all available examples for the current platform
if(EXISTS EXAMPLE_TARGETS)
    add_custom_target(build_all_examples ALL DEPENDS ${EXAMPLE_TARGETS})
endif()

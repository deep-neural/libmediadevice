cmake_minimum_required(VERSION 3.10)
project(VideoAudioCapture)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/video)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/audio)

# Define common source files for all platforms
set(COMMON_SOURCES
    media_device.cc
    media_device.h
)

# Define source files for different platforms
set(WINDOWS_SOURCES
    video/dxgi_video_device.cc
    video/dxgi_video_device.h
    audio/wasapi_audio_device.cc
    audio/wasapi_audio_device.h
)

set(LINUX_SOURCES
    video/nvfbc_video_device.cc
    video/nvfbc_video_device.h
    video/x11_video_device.cc
    video/x11_video_device.h
    audio/pulse_audio_device.cc
    audio/pulse_audio_device.h
)

# Create the library as a shared library on Windows and Linux
if(WIN32 OR (UNIX AND NOT APPLE))
    add_library(mediadevice_lib SHARED "")
else()
    add_library(mediadevice_lib "")
endif()

# Add common source files that work on all platforms
target_sources(mediadevice_lib PRIVATE ${COMMON_SOURCES})

# Set the output name to mediadevice (CMAKE will add lib prefix automatically)
# Also ensure the library goes to the main build directory
set_target_properties(mediadevice_lib PROPERTIES
    OUTPUT_NAME mediadevice
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
)

# Conditionally add platform-specific source files and dependencies
if(WIN32)
    target_sources(mediadevice_lib PRIVATE ${WINDOWS_SOURCES})
    target_link_libraries(mediadevice_lib PRIVATE dxgi d3d11)
    target_compile_definitions(mediadevice_lib PRIVATE NOMINMAX _WINDLL)
elseif(UNIX AND NOT APPLE)
    target_sources(mediadevice_lib PRIVATE ${LINUX_SOURCES})
    
    # Find required Linux dependencies
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCB REQUIRED xcb)
    pkg_check_modules(XCBSHM REQUIRED xcb-shm)
    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(PULSE REQUIRED libpulse)
    
    # Include directories for XCB, xcb-shm, X11, and PulseAudio
    target_include_directories(mediadevice_lib PRIVATE 
        ${XCB_INCLUDE_DIRS}
        ${XCBSHM_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
        ${PULSE_INCLUDE_DIRS}
    )
    
    # Link libraries for XCB, xcb-shm, X11, and PulseAudio
    target_link_libraries(mediadevice_lib PRIVATE 
        ${XCB_LIBRARIES}
        ${XCBSHM_LIBRARIES}
        ${X11_LIBRARIES}
        ${PULSE_LIBRARIES}
    )
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Installation rules
install(TARGETS mediadevice_lib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Export the include directories
target_include_directories(mediadevice_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Set additional compile options if needed
target_compile_options(mediadevice_lib PRIVATE 
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Build examples after the main library
add_subdirectory(examples)

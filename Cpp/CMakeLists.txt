cmake_minimum_required(VERSION 3.16)
project(UDPAudioStreamer VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags for optimization and warnings
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
    add_compile_options(/W4)
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required packages
find_package(Threads REQUIRED)

# Build PortAudio from source
set(PA_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared PortAudio library")
set(PA_BUILD_TESTS OFF CACHE BOOL "Build PortAudio tests")
set(PA_BUILD_EXAMPLES OFF CACHE BOOL "Build PortAudio examples")

# Suppress PortAudio warnings
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996 /wd4244 /wd4267")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations")
endif()

# Add PortAudio subdirectory
add_subdirectory(external/portaudio EXCLUDE_FROM_ALL)

message(STATUS "Building PortAudio from source (external/portaudio)")

# Platform-specific libraries
if(WIN32)
    set(PLATFORM_LIBS ws2_32 winmm)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LIBS)
elseif(APPLE)
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
    set(PLATFORM_LIBS ${COREAUDIO_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK} ${AUDIOUNIT_FRAMEWORK})
endif()

# Create executable
add_executable(udp_audio_streamer
    src/main.cpp
    src/UDPAudioStreamer.cpp
    src/AudioPlayer.cpp
    src/PacketParser.cpp
)

# Include directories
target_include_directories(udp_audio_streamer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/portaudio/include
)

# Link libraries
target_link_libraries(udp_audio_streamer PRIVATE
    portaudio
    Threads::Threads
    ${PLATFORM_LIBS}
)

# Installation
install(TARGETS udp_audio_streamer
    RUNTIME DESTINATION bin
)

# Optional: Create test sender executable
option(BUILD_TEST_SENDER "Build test UDP sender" ON)

if(BUILD_TEST_SENDER)
    add_executable(test_sender
        src/test_sender.cpp
    )
    
    target_include_directories(test_sender PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    target_link_libraries(test_sender PRIVATE
        Threads::Threads
        ${PLATFORM_LIBS}
    )
    
    install(TARGETS test_sender
        RUNTIME DESTINATION bin
    )
endif()

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Platform libraries: ${PLATFORM_LIBS}")

# Package configuration
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
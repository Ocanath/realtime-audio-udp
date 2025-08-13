# C++ UDP Audio Streamer

High-performance C++ implementation of the real-time UDP audio streaming system using PortAudio for low-latency audio playback.

## Key Features

- **Self-contained build** - PortAudio included as submodule, no external dependencies
- **Cross-platform** - Windows, macOS, Linux support  
- **Low latency** - Optimized for real-time audio streaming (~10-50ms)
- **Complete solution** - Includes both receiver and test sender

## Dependencies

### Required (Minimal!)
- **CMake** 3.16 or higher
- **C++17** compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **Git** (for cloning with submodules)

### Platform-Specific Compiler Installation

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake git
```

**CentOS/RHEL/Fedora:**
```bash
# CentOS/RHEL
sudo yum install gcc-c++ cmake git

# Fedora  
sudo dnf install gcc-c++ cmake git
```

**macOS:**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install CMake (if not already installed)
brew install cmake
```

**Windows:**
- Install Visual Studio 2017+ (Community Edition is free) or MinGW-w64
- Install CMake from https://cmake.org/
- Install Git for Windows from https://git-scm.com/

**That's it!** No need to install PortAudio separately - it's built automatically from source.

## Building

### Quick Start (All Platforms)

```bash
# Clone with submodules
git clone --recursive https://github.com/your-repo/realtime-audio-udp.git
cd realtime-audio-udp/Cpp

# Build
mkdir build && cd build
cmake ..
cmake --build . --config Release

# The executables will be in build/ (or build/Release/ on Windows)
```

### Platform-Specific Build Instructions

**Unix/Linux/macOS:**
```bash
cd Cpp
mkdir build && cd build
cmake ..
make -j$(nproc)  # Use all CPU cores

# Executables: ./udp_audio_streamer and ./test_sender
```

**Windows (Visual Studio):**
```cmd
cd Cpp
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release

# Executables: build\Release\udp_audio_streamer.exe and build\Release\test_sender.exe
```

**Windows (MinGW):**
```cmd
cd Cpp
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make

# Executables: build\udp_audio_streamer.exe and build\test_sender.exe
```

## Usage

### UDP Audio Receiver

```bash
# Basic usage
./udp_audio_streamer 8000

# Custom sample rate
./udp_audio_streamer 8000 --sample-rate 44100

# Save to WAV file
./udp_audio_streamer 8000 --save-file recording.wav

# Combined options
./udp_audio_streamer 8000 --sample-rate 16000 --save-file output.wav
```

### Test Sender (C++)

```bash
# Send test audio to receiver
./test_sender localhost 8000

# Custom frequency and sample rate
./test_sender localhost 8000 --frequency 880 --sample-rate 44100

# Shorter packets for lower latency
./test_sender localhost 8000 --packet-duration 0.01
```

### Testing Complete System

```bash
# Terminal 1: Start receiver
./udp_audio_streamer 8000

# Terminal 2: Send test audio
./test_sender localhost 8000
```

You should hear a 440Hz sine wave playing through your speakers!

## Build Configuration

### CMake Options

```bash
# Debug build with symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build with optimizations (default)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Disable test sender build
cmake .. -DBUILD_TEST_SENDER=OFF
```

### Submodule Management

If you cloned without `--recursive`, get the submodules:
```bash
git submodule update --init --recursive
```

To update PortAudio to latest version:
```bash
cd external/portaudio
git pull origin master
cd ../..
git add external/portaudio
git commit -m "Update PortAudio submodule"
```

## Troubleshooting

### Build Issues

**Error**: `fatal: No such file or directory: external/portaudio`
**Solution**: Initialize submodules:
```bash
git submodule update --init --recursive
```

**Error**: CMake can't find compiler
**Solution**: Install build tools for your platform (see Dependencies section)

**Error**: PortAudio build warnings
**Solution**: These are suppressed automatically and don't affect functionality

### Audio Issues

**Error**: `No default output device found`
**Solutions**:
1. Check audio devices are properly configured
2. On Linux, ensure user is in `audio` group:
   ```bash
   sudo usermod -a -G audio $USER
   # Log out and back in
   ```
3. Try a different audio sample rate:
   ```bash
   ./udp_audio_streamer 8000 --sample-rate 44100
   ```

**Error**: Audio dropouts or crackling
**Solutions**:
1. Use Release build for better performance
2. Close other audio applications
3. Try larger packet duration:
   ```bash
   ./test_sender localhost 8000 --packet-duration 0.05
   ```

### Network Issues

**Error**: `Bind failed` or `Address already in use`
**Solutions**:
1. Choose a different port number
2. Wait a few minutes if port was recently used
3. Check if another process is using the port:
   ```bash
   # Linux/macOS
   netstat -tulpn | grep :8000
   
   # Windows  
   netstat -an | findstr :8000
   ```

## Performance Tuning

### For Ultra-Low Latency

1. **Use Release build**:
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

2. **Reduce packet duration**:
   ```bash
   ./test_sender localhost 8000 --packet-duration 0.005  # 5ms packets
   ```

3. **Increase process priority** (Linux/macOS):
   ```bash
   sudo nice -n -10 ./udp_audio_streamer 8000
   ```

### For Memory-Constrained Systems

Modify these constants in the source and rebuild:
- `MAX_QUEUE_SIZE` in `AudioPlayer.h` - Reduce audio buffer size
- `FRAMES_PER_BUFFER` in `AudioPlayer.h` - Smaller PortAudio buffers

## Architecture

### Project Structure
```
Cpp/
├── CMakeLists.txt              # Build configuration
├── external/
│   └── portaudio/              # PortAudio submodule
├── include/
│   ├── UDPAudioStreamer.h      # Main coordinator
│   ├── AudioPlayer.h           # PortAudio interface
│   └── PacketParser.h          # Frame parsing
└── src/
    ├── main.cpp                # Receiver entry point
    ├── test_sender.cpp         # Test audio generator
    ├── UDPAudioStreamer.cpp    # Network handling
    ├── AudioPlayer.cpp         # Audio playback
    └── PacketParser.cpp        # Packet parsing
```

### Threading Model
- **Main thread**: Argument parsing, signal handling
- **UDP receiver thread**: Network packet reception
- **PortAudio callback thread**: Real-time audio output

### Key Design Decisions
- **Static linking**: PortAudio built as static library for easier deployment
- **Cross-platform sockets**: Unified interface for Windows/Unix networking
- **RAII resource management**: Automatic cleanup on destruction
- **Thread-safe queues**: Mutex-protected audio buffers

## Contributing

When modifying the C++ implementation:

1. **Test on multiple platforms** - Verify Windows, Linux, and macOS
2. **Maintain compatibility** - Keep frame format consistent with Python version
3. **Profile performance** - Use appropriate tools for your platform
4. **Check memory safety** - Use valgrind, AddressSanitizer, etc.
5. **Update documentation** - Keep README in sync with code changes

## Performance Comparison

| Metric | Python Implementation | C++ Implementation |
|--------|----------------------|-------------------|
| Latency | 50-200ms | 10-50ms |
| CPU Usage | Moderate-High | Low |
| Memory Usage | Higher | Lower |
| Setup Complexity | Medium | Low (with submodule) |
| Best For | Development/Testing | Production/Real-time |
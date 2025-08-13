# Real-time UDP Audio Streaming

A high-performance, cross-platform UDP audio streaming system designed for real-time audio transmission and playback. This project provides both Python and C++ implementations for receiving raw audio data over UDP and playing it back through the system's audio interface.

## Overview

This system implements a structured frame-based protocol for transmitting audio data over UDP networks. It features:

- **Real-time UDP audio streaming** with minimal latency
- **Structured packet format** including sequence numbers and timestamps
- **Packet loss detection** and statistics reporting
- **Cross-platform support** (Windows, macOS, Linux)
- **Multiple implementations** for different use cases

## Frame Format

Each UDP packet contains:
```
[2-byte sequence#][4-byte sample_timestamp][audio_samples]
```

- **Sequence number**: 16-bit little-endian, wraps at 65536
- **Sample timestamp**: 32-bit little-endian, absolute sample count since stream start
- **Audio samples**: Variable length, 16-bit little-endian signed integers (mono)

## Implementations

### Python Implementation (`python/`)
- **Purpose**: Rapid prototyping, testing, and development
- **Dependencies**: PyAudio
- **Best for**: Research, development, testing protocols
- **Latency**: ~50-200ms (acceptable for non-critical applications)

### C++ Implementation (`Cpp/`)
- **Purpose**: Production deployment and low-latency applications  
- **Dependencies**: CMake, C++17 compiler (PortAudio included as submodule)
- **Best for**: Real-time audio applications, embedded systems
- **Latency**: ~10-50ms (suitable for real-time applications)

## Quick Start

### Python Version
```bash
cd python
pip install -r requirements.txt

# Start receiver
python udp_audio_streamer.py 8000

# Send test audio (separate terminal)
python test_sender.py localhost 8000
```

### C++ Version
```bash
# Clone with submodules (important!)
git clone --recursive <repository-url>

cd Cpp
mkdir build && cd build
cmake ..
cmake --build . --config Release

# Start receiver
./udp_audio_streamer 8000

# Send test audio (C++ sender)
./test_sender localhost 8000
```

## Use Cases

- **Audio streaming** over local networks
- **Intercom systems** for low-latency communication
- **Audio testing** and protocol development
- **Real-time audio processing** pipeline development
- **Network audio research** and experimentation

## Performance Characteristics

| Implementation | Latency    | CPU Usage | Memory  | Setup Complexity | Best For           |
|---------------|------------|-----------|---------|------------------|-------------------|
| Python        | 50-200ms   | Moderate  | Higher  | Medium           | Development/Testing|
| C++           | 10-50ms    | Low       | Lower   | Low (submodule)  | Production/Real-time|

## Network Requirements

- **Bandwidth**: ~256 kbps at 16kHz (uncompressed mono 16-bit)
- **Latency**: <50ms network latency recommended
- **Packet loss**: <1% for acceptable audio quality
- **Jitter**: <50ms variation for smooth playback

## Project Structure

```
realtime-audio-udp/
├── README.md                    # This file
├── python/                      # Python implementation
│   ├── README.md               # Python-specific documentation
│   ├── udp_audio_streamer.py   # Main receiver
│   ├── test_sender.py          # Test audio generator
│   └── requirements.txt        # Python dependencies
└── Cpp/                        # C++ implementation
    ├── README.md               # C++ build instructions
    ├── CMakeLists.txt          # Build configuration
    ├── include/                # Header files
    └── src/                    # Source files
```

## Contributing

This project is designed for educational and research purposes. When contributing:

1. Test both Python and C++ implementations
2. Maintain compatibility with the frame format
3. Update documentation for any protocol changes
4. Consider performance implications for real-time use cases

## License

This software is provided as-is for educational and research purposes.
# Real-time UDP Audio Streamer

A Python-based real-time audio streaming system that receives raw audio data over UDP and plays it back through the computer's audio interface. The system uses a structured frame format with sequence numbers and timestamps for reliable audio transmission.

## Features

- **Real-time UDP audio reception** - Binds to any port and receives audio packets
- **Structured frame format** - Includes sequence numbers and sample timestamps
- **Packet loss detection** - Tracks dropped and out-of-order packets
- **Audio playback** - Real-time playback through system audio interface
- **File saving** - Optional WAV file recording
- **Configurable sample rate** - Default 16kHz, customizable
- **Jitter buffering** - Queue-based buffering to handle network variations
- **Statistics reporting** - Detailed packet loss and performance metrics

## Frame Format

Each UDP packet contains:
```
[2-byte sequence#][4-byte sample_timestamp][audio_samples]
```

- **Sequence number**: 16-bit little-endian, wraps at 65536
- **Sample timestamp**: 32-bit little-endian, absolute sample count since stream start  
- **Audio samples**: Variable length, 16-bit little-endian signed integers

## Requirements

- Python 3.5+
- PyAudio library

## Installation

1. Clone or download this repository
2. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```

## Usage

### Basic Audio Streaming

Start the UDP audio receiver:
```bash
python udp_audio_streamer.py <port>
```

Example:
```bash
python udp_audio_streamer.py 8000
```

### Custom Sample Rate

Specify a different sample rate:
```bash
python udp_audio_streamer.py 8000 --sample-rate 44100
```

### Save Audio to File

Record received audio to a WAV file:
```bash
python udp_audio_streamer.py 8000 --save-file recording.wav
```

### Complete Example

```bash
python udp_audio_streamer.py 8000 --sample-rate 16000 --save-file output.wav
```

## Testing

A test utility is included to generate sine wave audio packets:

```bash
python test_sender.py <host> <port>
```

Examples:
```bash
# Send 440Hz sine wave to localhost:8000
python test_sender.py localhost 8000

# Custom frequency and sample rate
python test_sender.py localhost 8000 --frequency 880 --sample-rate 44100

# Shorter packet duration (10ms packets instead of 20ms)
python test_sender.py localhost 8000 --packet-duration 0.01
```

## Command Line Options

### udp_audio_streamer.py
- `port` - UDP port to bind to (required)
- `--sample-rate` - Audio sample rate in Hz (default: 16000)
- `--save-file` - Save received audio to WAV file (optional)

### test_sender.py
- `host` - Target host/IP address (required)
- `port` - Target UDP port (required)
- `--sample-rate` - Audio sample rate in Hz (default: 16000)
- `--frequency` - Sine wave frequency in Hz (default: 440.0)
- `--packet-duration` - Duration of each packet in seconds (default: 0.02)

## Performance Considerations

- **Latency**: ~20-100ms depending on network conditions and buffer settings
- **Packet loss**: Detected and reported, but not recovered (no retransmission)
- **Jitter handling**: Basic queue-based buffering (100 packet maximum)
- **CPU usage**: Moderate due to Python overhead and real-time constraints

## Network Requirements

- **Bandwidth**: ~256 kbps at 16kHz (uncompressed 16-bit mono)
- **Latency**: Low-latency network recommended for real-time applications
- **Packet loss**: <1% for acceptable audio quality
- **Jitter**: <50ms variation for smooth playback

## Technical Details

### Threading Model
- **UDP receiver thread**: Handles incoming packets and parsing
- **Audio playback thread**: Manages real-time audio output
- **Main thread**: Coordinates startup/shutdown and user interaction

### Buffer Management
- Queue-based audio buffering with overflow protection
- Oldest packets dropped when queue is full
- Statistics tracking for performance monitoring

### Error Handling
- Graceful handling of malformed packets
- Network timeout management
- Clean shutdown on Ctrl+C

## Limitations

- **No packet reordering** - Out-of-order packets are played immediately
- **No error correction** - Lost packets result in audio gaps
- **Python performance** - Not suitable for ultra-low latency applications
- **Single stream** - Handles one audio stream at a time

## Future Enhancements

- Packet reordering buffer
- Forward error correction (FEC)
- Multiple simultaneous streams
- Compression support (Opus, etc.)
- C++ implementation for lower latency

## License

This software is provided as-is for educational and research purposes.
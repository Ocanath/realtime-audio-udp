#!/usr/bin/env python3

import argparse
import socket
import struct
import threading
import queue
import wave
import time
from typing import Optional

try:
    import pyaudio
except ImportError:
    print("PyAudio not installed. Please install with: pip install pyaudio")
    exit(1)


class UDPAudioStreamer:
    def __init__(self, port: int, sample_rate: int = 16000, save_file: Optional[str] = None):
        self.port = port
        self.sample_rate = sample_rate
        self.save_file = save_file
        self.running = False
        
        # Audio configuration
        self.channels = 1  # Mono
        self.sample_width = 2  # 16-bit = 2 bytes
        self.chunk_size = 1024
        
        # Threading and buffering
        self.audio_queue = queue.Queue(maxsize=100)
        self.udp_thread = None
        self.audio_thread = None
        
        # PyAudio setup
        self.pyaudio_instance = pyaudio.PyAudio()
        self.stream = None
        
        # File saving
        self.wav_file = None
        self.audio_data_buffer = []
        
        # Socket
        self.sock = None

    def setup_audio_stream(self):
        """Initialize PyAudio stream for playback"""
        self.stream = self.pyaudio_instance.open(
            format=pyaudio.paInt16,
            channels=self.channels,
            rate=self.sample_rate,
            output=True,
            frames_per_buffer=self.chunk_size
        )

    def setup_wav_file(self):
        """Initialize WAV file for saving if requested"""
        if self.save_file:
            self.wav_file = wave.open(self.save_file, 'wb')
            self.wav_file.setnchannels(self.channels)
            self.wav_file.setsampwidth(self.sample_width)
            self.wav_file.setframerate(self.sample_rate)

    def udp_receiver(self):
        """UDP packet receiver thread"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(('0.0.0.0', self.port))
        self.sock.settimeout(1.0)
        
        print(f"UDP Audio Streamer listening on 0.0.0.0:{self.port}")
        print(f"Sample rate: {self.sample_rate} Hz")
        if self.save_file:
            print(f"Saving audio to: {self.save_file}")
        
        while self.running:
            try:
                data, addr = self.sock.recvfrom(4096)
                if data and len(data) >= 2:
                    # Parse little-endian 16-bit samples
                    samples = struct.unpack(f'<{len(data)//2}h', data)
                    audio_bytes = struct.pack(f'{len(samples)}h', *samples)
                    
                    # Add to playback queue
                    try:
                        self.audio_queue.put_nowait(audio_bytes)
                    except queue.Full:
                        # Drop oldest packet if queue is full
                        try:
                            self.audio_queue.get_nowait()
                            self.audio_queue.put_nowait(audio_bytes)
                        except queue.Empty:
                            pass
                    
                    # Save to file if requested
                    if self.save_file:
                        self.audio_data_buffer.append(audio_bytes)
                        
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"UDP receiver error: {e}")

    def audio_player(self):
        """Audio playback thread"""
        while self.running:
            try:
                audio_data = self.audio_queue.get(timeout=1.0)
                if self.stream and audio_data:
                    self.stream.write(audio_data)
            except queue.Empty:
                continue
            except Exception as e:
                if self.running:
                    print(f"Audio playback error: {e}")

    def start(self):
        """Start the UDP audio streamer"""
        self.running = True
        
        # Setup audio and file saving
        self.setup_audio_stream()
        self.setup_wav_file()
        
        # Start threads
        self.udp_thread = threading.Thread(target=self.udp_receiver, daemon=True)
        self.audio_thread = threading.Thread(target=self.audio_player, daemon=True)
        
        self.udp_thread.start()
        self.audio_thread.start()
        
        try:
            # Keep main thread alive
            while self.running:
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\nShutting down...")
            self.stop()

    def stop(self):
        """Stop the UDP audio streamer"""
        self.running = False
        
        # Wait for threads to finish
        if self.udp_thread:
            self.udp_thread.join(timeout=2.0)
        if self.audio_thread:
            self.audio_thread.join(timeout=2.0)
        
        # Close audio stream
        if self.stream:
            self.stream.stop_stream()
            self.stream.close()
        self.pyaudio_instance.terminate()
        
        # Close socket
        if self.sock:
            self.sock.close()
        
        # Save buffered audio data to file
        if self.wav_file and self.audio_data_buffer:
            print(f"Writing {len(self.audio_data_buffer)} audio chunks to {self.save_file}")
            for chunk in self.audio_data_buffer:
                self.wav_file.writeframes(chunk)
            self.wav_file.close()
            print(f"Audio saved to {self.save_file}")


def main():
    parser = argparse.ArgumentParser(description="Real-time UDP Audio Streamer")
    parser.add_argument('port', type=int, help='UDP port to bind to')
    parser.add_argument('--sample-rate', type=int, default=16000, 
                       help='Audio sample rate in Hz (default: 16000)')
    parser.add_argument('--save-file', type=str, default=None,
                       help='Save received audio to WAV file (optional)')
    
    args = parser.parse_args()
    
    # Validate arguments
    if args.port < 1 or args.port > 65535:
        print("Error: Port must be between 1 and 65535")
        return 1
    
    if args.sample_rate <= 0:
        print("Error: Sample rate must be positive")
        return 1
    
    # Create and start streamer
    streamer = UDPAudioStreamer(args.port, args.sample_rate, args.save_file)
    
    try:
        streamer.start()
    except Exception as e:
        print(f"Error starting streamer: {e}")
        return 1
    
    return 0


if __name__ == "__main__":
    exit(main())
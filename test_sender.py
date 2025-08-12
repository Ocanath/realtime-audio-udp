#!/usr/bin/env python3

import argparse
import socket
import struct
import time
import math


def generate_continuous_sine_wave(frequency: float, sample_rate: int, duration: float, amplitude: float = 0.3):
    """Generate continuous sine wave audio samples"""
    num_samples = int(sample_rate * duration)
    samples = []
    
    for i in range(num_samples):
        t = i / sample_rate
        sample = amplitude * math.sin(2 * math.pi * frequency * t)
        # Convert to 16-bit signed integer
        sample_int = int(sample * 32767)
        # Clamp to 16-bit range
        sample_int = max(-32768, min(32767, sample_int))
        samples.append(sample_int)
    
    return samples


def send_audio_packets(host: str, port: int, sample_rate: int = 16000, 
                      tone_frequency: float = 440.0, packet_duration: float = 0.02,
                      buffer_duration: float = 3.0):
    """Send UDP audio packets with a continuous sine wave tone"""
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print(f"Sending audio packets to {host}:{port}")
    print(f"Sample rate: {sample_rate} Hz")
    print(f"Tone frequency: {tone_frequency} Hz")
    print(f"Packet duration: {packet_duration} seconds")
    print(f"Buffer duration: {buffer_duration} seconds")
    print(f"Frame format: [2-byte seq#][4-byte sample timestamp][audio samples]")
    print("Press Ctrl+C to stop")
    
    # Calculate packet parameters
    samples_per_packet = int(sample_rate * packet_duration)
    buffer_samples = int(sample_rate * buffer_duration)
    packets_per_buffer = buffer_samples // samples_per_packet
    
    print(f"Samples per packet: {samples_per_packet}")
    print(f"Packets per buffer: {packets_per_buffer}")
    
    try:
        packet_count = 0
        seq_num = 0
        sample_timestamp = 0
        buffer_start_time = 0.0  # Time offset for continuous sine wave
        
        while True:
            # Generate continuous audio buffer
            # print(f"Generating {buffer_duration}s audio buffer...")
            audio_buffer = generate_continuous_sine_wave(
                tone_frequency, sample_rate, buffer_duration
            )
            
            # Send packets from the buffer
            for packet_idx in range(packets_per_buffer):
                start_sample = packet_idx * samples_per_packet
                end_sample = start_sample + samples_per_packet
                packet_samples = audio_buffer[start_sample:end_sample]
                
                # Pack frame header: [2 bytes seq#][4 bytes sample_timestamp]
                header = struct.pack('<HI', seq_num, sample_timestamp)
                
                # Pack audio samples as little-endian 16-bit integers
                audio_data = struct.pack(f'<{len(packet_samples)}h', *packet_samples)
                
                # Combine header and audio data
                packet_data = header + audio_data
                
                # Send packet
                sock.sendto(packet_data, (host, port))
                packet_count += 1
                
                # Update counters for next packet
                seq_num = (seq_num + 1) % 65536  # Wrap at 16-bit boundary
                sample_timestamp += len(packet_samples)
                
                # if packet_count % 50 == 0:  # Print status every 50 packets
                #     print(f"Sent {packet_count} packets (seq: {seq_num-1}, timestamp: {sample_timestamp-len(packet_samples)})...")
                
                # Sleep to maintain real-time rate
            
            # Update buffer start time for next continuous buffer
            buffer_start_time += buffer_duration
            # time.sleep(packet_duration)
            
    except KeyboardInterrupt:
        print(f"\nSent {packet_count} total packets")
        print(f"Final sequence number: {seq_num-1}")
        print(f"Final sample timestamp: {sample_timestamp}")
    finally:
        sock.close()


def main():
    parser = argparse.ArgumentParser(description="UDP Audio Packet Sender (Test Tool)")
    parser.add_argument('host', help='Target host/IP address')
    parser.add_argument('port', type=int, help='Target UDP port')
    parser.add_argument('--sample-rate', type=int, default=16000,
                       help='Audio sample rate in Hz (default: 16000)')
    parser.add_argument('--frequency', type=float, default=440.0,
                       help='Sine wave frequency in Hz (default: 440.0)')
    parser.add_argument('--packet-duration', type=float, default=0.02,
                       help='Duration of each packet in seconds (default: 0.02)')
    parser.add_argument('--buffer-duration', type=float, default=3.0,
                       help='Duration of continuous audio buffer in seconds (default: 3.0)')
    
    args = parser.parse_args()
    
    # Validate arguments
    if args.port < 1 or args.port > 65535:
        print("Error: Port must be between 1 and 65535")
        return 1
    
    if args.sample_rate <= 0:
        print("Error: Sample rate must be positive")
        return 1
    
    if args.frequency <= 0:
        print("Error: Frequency must be positive")
        return 1
    
    if args.packet_duration <= 0:
        print("Error: Packet duration must be positive")
        return 1
    
    send_audio_packets(args.host, args.port, args.sample_rate, 
                      args.frequency, args.packet_duration, args.buffer_duration)
    
    return 0


if __name__ == "__main__":
    exit(main())
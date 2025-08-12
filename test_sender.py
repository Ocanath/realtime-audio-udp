#!/usr/bin/env python3

import argparse
import socket
import struct
import time
import math


def generate_sine_wave_samples(frequency: float, sample_rate: int, duration: float, amplitude: float = 0.3):
    """Generate sine wave audio samples"""
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
                      tone_frequency: float = 440.0, packet_duration: float = 0.02):
    """Send UDP audio packets with a sine wave tone"""
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print(f"Sending audio packets to {host}:{port}")
    print(f"Sample rate: {sample_rate} Hz")
    print(f"Tone frequency: {tone_frequency} Hz")
    print(f"Packet duration: {packet_duration} seconds")
    print("Press Ctrl+C to stop")
    
    try:
        packet_count = 0
        while True:
            # Generate audio samples for this packet
            samples = generate_sine_wave_samples(tone_frequency, sample_rate, packet_duration)
            
            # Pack samples as little-endian 16-bit integers
            packet_data = struct.pack(f'<{len(samples)}h', *samples)
            
            # Send packet
            sock.sendto(packet_data, (host, port))
            packet_count += 1
            
            if packet_count % 50 == 0:  # Print status every 50 packets (1 second at 20ms packets)
                print(f"Sent {packet_count} packets...")
            
            # Sleep to maintain real-time rate
            time.sleep(packet_duration)
            
    except KeyboardInterrupt:
        print(f"\nSent {packet_count} total packets")
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
                      args.frequency, args.packet_duration)
    
    return 0


if __name__ == "__main__":
    exit(main())
#include "PacketParser.h"
#include <cstring>
#include <iostream>

PacketParser::PacketParser() {
    resetStats();
}

std::optional<AudioPacket> PacketParser::parsePacket(const uint8_t* data, size_t length) {
    // Minimum packet size: 2 bytes seq + 4 bytes timestamp + at least 2 bytes audio
    if (length < 8 || data == nullptr) {
        return std::nullopt;
    }

    // Parse header: [2 bytes seq#][4 bytes sample_timestamp]
    uint16_t sequenceNumber;
    uint32_t sampleTimestamp;
    
    // Read little-endian values
    std::memcpy(&sequenceNumber, data, 2);
    std::memcpy(&sampleTimestamp, data + 2, 4);
    
    // Convert from little-endian if necessary (assuming host is little-endian for simplicity)
    // In production, use proper endianness conversion functions
    
    // Extract audio data
    size_t audioDataLength = length - 6;  // Remaining bytes after header
    
    // Audio data must be even number of bytes (16-bit samples)
    if (audioDataLength % 2 != 0) {
        std::cerr << "Warning: Invalid audio data length " << audioDataLength 
                  << " (must be even)" << std::endl;
        return std::nullopt;
    }
    
    size_t numSamples = audioDataLength / 2;
    std::vector<int16_t> audioSamples(numSamples);
    
    // Parse 16-bit little-endian audio samples
    const uint8_t* audioData = data + 6;
    for (size_t i = 0; i < numSamples; ++i) {
        std::memcpy(&audioSamples[i], audioData + (i * 2), 2);
    }
    
    // Update statistics
    updateStatistics(sequenceNumber);
    
    return AudioPacket(sequenceNumber, sampleTimestamp, std::move(audioSamples));
}

void PacketParser::updateStatistics(uint16_t sequenceNumber) {
    stats_.totalReceived++;
    
    if (!stats_.firstPacketReceived) {
        stats_.lastSequenceNumber = sequenceNumber;
        stats_.firstPacketReceived = true;
        return;
    }
    
    uint16_t expectedSeq = static_cast<uint16_t>(stats_.lastSequenceNumber + 1);
    
    if (sequenceNumber != expectedSeq) {
        if (isSequenceNumberValid(sequenceNumber, expectedSeq)) {
            // Packets were dropped
            uint16_t dropped;
            if (sequenceNumber > expectedSeq) {
                dropped = sequenceNumber - expectedSeq;
            } else {
                // Sequence number wrapped around
                dropped = (65536 - expectedSeq) + sequenceNumber;
            }
            
            stats_.totalDropped += dropped;
            std::cout << "Warning: " << dropped << " packet(s) dropped (seq " 
                      << expectedSeq << " to " << (sequenceNumber - 1) << ")" << std::endl;
        } else {
            // Out of order packet
            stats_.outOfOrder++;
            std::cout << "Warning: Out of order packet (seq " << sequenceNumber 
                      << ", expected " << expectedSeq << ")" << std::endl;
        }
    }
    
    stats_.lastSequenceNumber = sequenceNumber;
}

bool PacketParser::isSequenceNumberValid(uint16_t current, uint16_t expected) const {
    // Handle wraparound: if expected > 32768 and current < 32768, likely wrapped
    if (expected > 32768 && current < 32768) {
        return true;  // Likely dropped packets with wraparound
    }
    
    // Normal case: current should be >= expected for dropped packets
    return current > expected;
}

void PacketParser::resetStats() {
    stats_ = PacketStats{};
}
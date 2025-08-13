#pragma once

#include <vector>
#include <cstdint>
#include <optional>

struct AudioPacket {
    uint16_t sequenceNumber;
    uint32_t sampleTimestamp;
    std::vector<int16_t> audioSamples;
    
    AudioPacket(uint16_t seq, uint32_t timestamp, std::vector<int16_t> samples)
        : sequenceNumber(seq), sampleTimestamp(timestamp), audioSamples(std::move(samples)) {}
};

class PacketParser {
public:
    PacketParser();
    ~PacketParser() = default;

    // Parse UDP packet data into AudioPacket
    std::optional<AudioPacket> parsePacket(const uint8_t* data, size_t length);
    
    // Packet tracking and statistics
    struct PacketStats {
        uint64_t totalReceived = 0;
        uint64_t totalDropped = 0;
        uint64_t outOfOrder = 0;
        uint16_t lastSequenceNumber = 0;
        bool firstPacketReceived = false;
    };

    const PacketStats& getStats() const { return stats_; }
    void resetStats();

private:
    PacketStats stats_;
    
    void updateStatistics(uint16_t sequenceNumber);
    bool isSequenceNumberValid(uint16_t current, uint16_t expected) const;
};
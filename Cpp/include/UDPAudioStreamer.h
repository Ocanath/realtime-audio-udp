#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdint>

class AudioPlayer;
class PacketParser;

class UDPAudioStreamer {
public:
    UDPAudioStreamer(int port, int sampleRate = 16000, const std::string& saveFile = "");
    ~UDPAudioStreamer();

    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // Statistics
    struct Statistics {
        uint64_t packetsReceived = 0;
        uint64_t packetsDropped = 0;
        uint64_t packetsOutOfOrder = 0;
        uint64_t bytesReceived = 0;
        double dropRate = 0.0;
    };

    Statistics getStatistics() const { return stats_; }

private:
    void udpReceiverThread();
    bool initializeSocket();
    void cleanup();

    int port_;
    int sampleRate_;
    std::string saveFile_;

    std::atomic<bool> running_{false};
    std::thread udpThread_;

    std::unique_ptr<AudioPlayer> audioPlayer_;
    std::unique_ptr<PacketParser> packetParser_;

    // Socket handle (platform-specific)
#ifdef _WIN32
    uintptr_t socket_ = 0;  // SOCKET on Windows
#else
    int socket_ = -1;       // file descriptor on Unix
#endif

    Statistics stats_;
    mutable std::mutex statsMutex_;
};
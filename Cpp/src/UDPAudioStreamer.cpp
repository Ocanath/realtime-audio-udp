#include "UDPAudioStreamer.h"
#include "AudioPlayer.h"
#include "PacketParser.h"
#include <iostream>
#include <chrono>
#include <mutex>
#include <iomanip>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

UDPAudioStreamer::UDPAudioStreamer(int port, int sampleRate, const std::string& saveFile)
    : port_(port), sampleRate_(sampleRate), saveFile_(saveFile) {
    
    audioPlayer_ = std::make_unique<AudioPlayer>(sampleRate, saveFile);
    packetParser_ = std::make_unique<PacketParser>();
}

UDPAudioStreamer::~UDPAudioStreamer() {
    stop();
}

bool UDPAudioStreamer::start() {
    if (running_.load()) {
        std::cerr << "Streamer is already running" << std::endl;
        return false;
    }

    // Initialize audio player
    if (!audioPlayer_->initialize()) {
        std::cerr << "Failed to initialize audio player" << std::endl;
        return false;
    }

    // Initialize UDP socket
    if (!initializeSocket()) {
        std::cerr << "Failed to initialize UDP socket" << std::endl;
        audioPlayer_->shutdown();
        return false;
    }

    running_.store(true);

    // Start UDP receiver thread
    udpThread_ = std::thread(&UDPAudioStreamer::udpReceiverThread, this);

    std::cout << "UDP Audio Streamer started on port " << port_ << std::endl;
    std::cout << "Sample rate: " << sampleRate_ << " Hz" << std::endl;
    std::cout << "Frame format: [2-byte seq#][4-byte sample timestamp][audio samples]" << std::endl;
    if (!saveFile_.empty()) {
        std::cout << "Saving audio to: " << saveFile_ << std::endl;
    }

    return true;
}

void UDPAudioStreamer::stop() {
    if (!running_.load()) return;

    running_.store(false);

    // Wait for UDP thread to finish
    if (udpThread_.joinable()) {
        udpThread_.join();
    }

    // Print statistics
    auto parserStats = packetParser_->getStats();
    if (parserStats.totalReceived > 0) {
        std::cout << "\nPacket Statistics:" << std::endl;
        std::cout << "  Packets received: " << parserStats.totalReceived << std::endl;
        std::cout << "  Packets dropped: " << parserStats.totalDropped << std::endl;
        std::cout << "  Packets out of order: " << parserStats.outOfOrder << std::endl;
        
        uint64_t totalPackets = parserStats.totalReceived + parserStats.totalDropped;
        if (totalPackets > 0) {
            double dropRate = (static_cast<double>(parserStats.totalDropped) / totalPackets) * 100.0;
            std::cout << "  Drop rate: " << std::fixed << std::setprecision(2) << dropRate << "%" << std::endl;
        }
    }

    // Cleanup
    cleanup();
    audioPlayer_->shutdown();

    std::cout << "UDP Audio Streamer stopped" << std::endl;
}

bool UDPAudioStreamer::initializeSocket() {
#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }

    // Create socket
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    // Set socket to reuse address
    BOOL reuse = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: " << WSAGetLastError() << std::endl;
    }

    // Bind socket
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<u_short>(port_));

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        WSACleanup();
        return false;
    }

    // Set socket timeout
    DWORD timeout = 1000; // 1 second
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        std::cerr << "Failed to set socket timeout: " << WSAGetLastError() << std::endl;
    }

    socket_ = sock;

#else
    // Unix/Linux socket implementation
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return false;
    }

    // Set socket to reuse address
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    // Bind socket
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return false;
    }

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Failed to set socket timeout");
    }

    socket_ = sock;
#endif

    return true;
}

void UDPAudioStreamer::udpReceiverThread() {
    const size_t BUFFER_SIZE = 4096;
    uint8_t buffer[BUFFER_SIZE];

    while (running_.load()) {
#ifdef _WIN32
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        int bytesReceived = recvfrom(static_cast<SOCKET>(socket_), 
                                   reinterpret_cast<char*>(buffer), 
                                   BUFFER_SIZE, 0,
                                   (sockaddr*)&clientAddr, &clientAddrLen);

        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                continue; // Timeout, check running flag again
            }
            if (running_.load()) {
                std::cerr << "UDP receive error: " << error << std::endl;
            }
            continue;
        }
#else
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        ssize_t bytesReceived = recvfrom(socket_, buffer, BUFFER_SIZE, 0,
                                       (struct sockaddr*)&clientAddr, &clientAddrLen);

        if (bytesReceived < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Timeout, check running flag again
            }
            if (running_.load()) {
                perror("UDP receive error");
            }
            continue;
        }
#endif

        if (bytesReceived > 0 && running_.load()) {
            // Parse the packet
            auto packet = packetParser_->parsePacket(buffer, static_cast<size_t>(bytesReceived));
            if (packet.has_value()) {
                // Add audio data to player
                audioPlayer_->addAudioData(packet->audioSamples);

                // Update statistics
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.packetsReceived++;
                stats_.bytesReceived += bytesReceived;
            }
        }
    }
}

void UDPAudioStreamer::cleanup() {
#ifdef _WIN32
    if (socket_ != 0) {
        closesocket(static_cast<SOCKET>(socket_));
        socket_ = 0;
        WSACleanup();
    }
#else
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
#endif
}
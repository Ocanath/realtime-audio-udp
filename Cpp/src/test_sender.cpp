#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <cstring>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

class UDPTestSender {
public:
    UDPTestSender(const std::string& host, int port, int sampleRate = 16000, 
                  double frequency = 440.0, double packetDuration = 0.02)
        : host_(host), port_(port), sampleRate_(sampleRate), 
          frequency_(frequency), packetDuration_(packetDuration) {
#ifdef _WIN32
        socket_ = INVALID_SOCKET;
#else
        socket_ = -1;
#endif
    }

    ~UDPTestSender() {
        cleanup();
    }

    bool initialize() {
#ifdef _WIN32
        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            return false;
        }

        // Create socket
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
            WSACleanup();
            return false;
        }
#else
        socket_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_ < 0) {
            perror("Socket creation failed");
            return false;
        }
#endif

        // Setup destination address
        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(static_cast<uint16_t>(port_));
        
#ifdef _WIN32
        if (inet_pton(AF_INET, host_.c_str(), &addr_.sin_addr) <= 0) {
            std::cerr << "Invalid address: " << host_ << std::endl;
            return false;
        }
#else
        if (inet_pton(AF_INET, host_.c_str(), &addr_.sin_addr) <= 0) {
            perror("Invalid address");
            return false;
        }
#endif

        return true;
    }

    void sendAudioPackets() {
        if (!initialize()) {
            std::cerr << "Failed to initialize sender" << std::endl;
            return;
        }

        std::cout << "Sending audio packets to " << host_ << ":" << port_ << std::endl;
        std::cout << "Sample rate: " << sampleRate_ << " Hz" << std::endl;
        std::cout << "Tone frequency: " << frequency_ << " Hz" << std::endl;
        std::cout << "Packet duration: " << packetDuration_ << " seconds" << std::endl;
        std::cout << "Frame format: [2-byte seq#][4-byte sample timestamp][audio samples]" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;

        int samplesPerPacket = static_cast<int>(sampleRate_ * packetDuration_);
        uint16_t sequenceNumber = 0;
        uint32_t sampleTimestamp = 0;
        uint64_t packetCount = 0;

        auto startTime = std::chrono::high_resolution_clock::now();
        auto nextPacketTime = startTime;

        try {
            while (true) {
                // Generate sine wave samples for this packet
                std::vector<int16_t> samples = generateSineWave(samplesPerPacket, sampleTimestamp);

                // Create packet: [2 bytes seq][4 bytes timestamp][audio samples]
                std::vector<uint8_t> packet(6 + samples.size() * 2);
                
                // Pack header (little-endian)
                std::memcpy(packet.data(), &sequenceNumber, 2);
                std::memcpy(packet.data() + 2, &sampleTimestamp, 4);
                
                // Pack audio samples (little-endian)
                std::memcpy(packet.data() + 6, samples.data(), samples.size() * 2);

                // Send packet
#ifdef _WIN32
                int result = sendto(socket_, reinterpret_cast<const char*>(packet.data()), 
                                  static_cast<int>(packet.size()), 0,
                                  reinterpret_cast<const sockaddr*>(&addr_), sizeof(addr_));
                if (result == SOCKET_ERROR) {
                    std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
                    break;
                }
#else
                ssize_t result = sendto(socket_, packet.data(), packet.size(), 0,
                                      reinterpret_cast<const sockaddr*>(&addr_), sizeof(addr_));
                if (result < 0) {
                    perror("Send failed");
                    break;
                }
#endif

                // Update counters
                sequenceNumber++;
                sampleTimestamp += samplesPerPacket;
                packetCount++;

                // Status update every 50 packets
                if (packetCount % 50 == 0) {
                    std::cout << "Sent " << packetCount << " packets (seq: " 
                              << (sequenceNumber - 1) << ", timestamp: " 
                              << (sampleTimestamp - samplesPerPacket) << ")..." << std::endl;
                }

                // Wait for next packet time
                nextPacketTime += std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
                    std::chrono::duration<double>(packetDuration_));
                
                std::this_thread::sleep_until(nextPacketTime);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        std::cout << "\nSent " << packetCount << " total packets" << std::endl;
        std::cout << "Final sequence number: " << (sequenceNumber - 1) << std::endl;
        std::cout << "Final sample timestamp: " << (sampleTimestamp - samplesPerPacket) << std::endl;
    }

private:
    std::vector<int16_t> generateSineWave(int numSamples, uint32_t startingSampleIndex) {
        std::vector<int16_t> samples(numSamples);
        const double amplitude = 0.3;
        
        for (int i = 0; i < numSamples; ++i) {
            double t = static_cast<double>(startingSampleIndex + i) / sampleRate_;
            double sample = amplitude * std::sin(2.0 * M_PI * frequency_ * t);
            
            // Convert to 16-bit signed integer
            int32_t sampleInt = static_cast<int32_t>(sample * 32767.0);
            // Clamp to 16-bit range
            if (sampleInt > 32767) sampleInt = 32767;
            if (sampleInt < -32768) sampleInt = -32768;
            samples[i] = static_cast<int16_t>(sampleInt);
        }
        
        return samples;
    }

    void cleanup() {
#ifdef _WIN32
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            WSACleanup();
        }
#else
        if (socket_ >= 0) {
            close(socket_);
            socket_ = -1;
        }
#endif
    }

    std::string host_;
    int port_;
    int sampleRate_;
    double frequency_;
    double packetDuration_;

#ifdef _WIN32
    SOCKET socket_;
#else
    int socket_;
#endif
    sockaddr_in addr_;
};

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <host> <port> [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --sample-rate <rate>      Audio sample rate in Hz (default: 16000)" << std::endl;
    std::cout << "  --frequency <freq>        Sine wave frequency in Hz (default: 440.0)" << std::endl;
    std::cout << "  --packet-duration <dur>   Duration of each packet in seconds (default: 0.02)" << std::endl;
    std::cout << "  --help                   Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " localhost 8000" << std::endl;
    std::cout << "  " << programName << " 192.168.1.100 8000 --frequency 880" << std::endl;
    std::cout << "  " << programName << " localhost 8000 --sample-rate 44100 --packet-duration 0.01" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string host;
    int port = 0;
    int sampleRate = 16000;
    double frequency = 440.0;
    double packetDuration = 0.02;

    // Parse command line arguments
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--sample-rate") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --sample-rate requires a value" << std::endl;
                return 1;
            }
            try {
                sampleRate = std::stoi(argv[++i]);
                if (sampleRate <= 0) {
                    std::cerr << "Error: Sample rate must be positive" << std::endl;
                    return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid sample rate: " << argv[i] << std::endl;
                return 1;
            }
        } else if (arg == "--frequency") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --frequency requires a value" << std::endl;
                return 1;
            }
            try {
                frequency = std::stod(argv[++i]);
                if (frequency <= 0) {
                    std::cerr << "Error: Frequency must be positive" << std::endl;
                    return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid frequency: " << argv[i] << std::endl;
                return 1;
            }
        } else if (arg == "--packet-duration") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --packet-duration requires a value" << std::endl;
                return 1;
            }
            try {
                packetDuration = std::stod(argv[++i]);
                if (packetDuration <= 0) {
                    std::cerr << "Error: Packet duration must be positive" << std::endl;
                    return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid packet duration: " << argv[i] << std::endl;
                return 1;
            }
        } else if (host.empty()) {
            host = arg;
        } else if (port == 0) {
            try {
                port = std::stoi(arg);
                if (port < 1 || port > 65535) {
                    std::cerr << "Error: Port must be between 1 and 65535" << std::endl;
                    return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid port: " << arg << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    if (host.empty() || port == 0) {
        std::cerr << "Error: Host and port are required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Create and run sender
    UDPTestSender sender(host, port, sampleRate, frequency, packetDuration);
    sender.sendAudioPackets();

    return 0;
}
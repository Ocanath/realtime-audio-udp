#include "UDPAudioStreamer.h"
#include <iostream>
#include <string>
#include <csignal>
#include <memory>
#include <thread>
#include <chrono>

std::unique_ptr<UDPAudioStreamer> g_streamer;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_streamer) {
        g_streamer->stop();
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <port> [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --sample-rate <rate>  Audio sample rate in Hz (default: 16000)" << std::endl;
    std::cout << "  --save-file <file>    Save received audio to WAV file (optional)" << std::endl;
    std::cout << "  --help               Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " 8000" << std::endl;
    std::cout << "  " << programName << " 8000 --sample-rate 44100" << std::endl;
    std::cout << "  " << programName << " 8000 --save-file recording.wav" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    int port = 0;
    int sampleRate = 16000;
    std::string saveFile;

    // Parse command line arguments
    if (argc < 2) {
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
        } else if (arg == "--save-file") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --save-file requires a filename" << std::endl;
                return 1;
            }
            saveFile = argv[++i];
        } else if (port == 0) {
            // First non-option argument should be the port
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

    if (port == 0) {
        std::cerr << "Error: Port is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Create and start the streamer
    std::cout << "Starting UDP Audio Streamer..." << std::endl;
    
    try {
        g_streamer = std::make_unique<UDPAudioStreamer>(port, sampleRate, saveFile);
        
        if (!g_streamer->start()) {
            std::cerr << "Failed to start UDP Audio Streamer" << std::endl;
            return 1;
        }

        std::cout << "Press Ctrl+C to stop..." << std::endl;

        // Keep the main thread alive while the streamer runs
        while (g_streamer->isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "UDP Audio Streamer finished" << std::endl;
    return 0;
}
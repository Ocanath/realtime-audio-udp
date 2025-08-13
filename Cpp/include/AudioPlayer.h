#pragma once

#include <portaudio.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <cstdint>

class AudioPlayer {
public:
    AudioPlayer(int sampleRate = 16000, const std::string& saveFile = "");
    ~AudioPlayer();

    bool initialize();
    void shutdown();
    
    bool addAudioData(const std::vector<int16_t>& samples);
    void flush();  // For saving remaining data to file

    bool isInitialized() const { return initialized_; }
    size_t getQueueSize() const;

private:
    static int audioCallback(const void* inputBuffer, void* outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData);

    int fillAudioBuffer(int16_t* output, unsigned long frameCount);

    int sampleRate_;
    std::string saveFile_;
    bool initialized_ = false;

    PaStream* stream_ = nullptr;
    
    // Audio buffer management
    std::queue<int16_t> audioQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    
    static constexpr size_t MAX_QUEUE_SIZE = 48000;  // ~3 seconds at 16kHz
    static constexpr int FRAMES_PER_BUFFER = 256;    // PortAudio buffer size

    // File saving
    std::unique_ptr<std::ofstream> wavFile_;
    std::vector<int16_t> fileBuffer_;
    void initializeWavFile();
    void writeWavHeader();
    void finalizeWavFile();
    uint32_t totalSamplesWritten_ = 0;
};
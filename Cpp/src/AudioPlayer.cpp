#include "AudioPlayer.h"
#include <iostream>
#include <cstring>
#include <algorithm>

AudioPlayer::AudioPlayer(int sampleRate, const std::string& saveFile)
    : sampleRate_(sampleRate), saveFile_(saveFile) {
}

AudioPlayer::~AudioPlayer() {
    shutdown();
}

bool AudioPlayer::initialize() {
    // Initialize PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    // Setup output stream parameters
    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        std::cerr << "No default output device found" << std::endl;
        Pa_Terminate();
        return false;
    }

    outputParameters.channelCount = 1;  // Mono
    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;

    // Open audio stream
    err = Pa_OpenStream(&stream_,
                        nullptr,              // no input
                        &outputParameters,
                        sampleRate_,
                        FRAMES_PER_BUFFER,
                        paClipOff,           // no clipping
                        audioCallback,
                        this);               // user data

    if (err != paNoError) {
        std::cerr << "Failed to open PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return false;
    }

    // Start the stream
    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "Failed to start PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream_);
        Pa_Terminate();
        return false;
    }

    // Initialize WAV file if requested
    if (!saveFile_.empty()) {
        initializeWavFile();
    }

    initialized_ = true;
    std::cout << "Audio player initialized (sample rate: " << sampleRate_ << " Hz)" << std::endl;
    
    return true;
}

void AudioPlayer::shutdown() {
    if (!initialized_) return;

    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }

    Pa_Terminate();

    // Finalize WAV file
    if (wavFile_) {
        finalizeWavFile();
    }

    initialized_ = false;
    std::cout << "Audio player shutdown" << std::endl;
}

bool AudioPlayer::addAudioData(const std::vector<int16_t>& samples) {
    if (!initialized_ || samples.empty()) return false;

    std::lock_guard<std::mutex> lock(queueMutex_);
    
    // Check queue size limit
    if (audioQueue_.size() + samples.size() > MAX_QUEUE_SIZE) {
        // Drop oldest samples to make room
        size_t dropCount = (audioQueue_.size() + samples.size()) - MAX_QUEUE_SIZE;
        for (size_t i = 0; i < dropCount && !audioQueue_.empty(); ++i) {
            audioQueue_.pop();
        }
        std::cout << "Warning: Audio buffer overflow, dropped " << dropCount << " samples" << std::endl;
    }

    // Add new samples to queue
    for (int16_t sample : samples) {
        audioQueue_.push(sample);
    }

    // Save to file if enabled
    if (wavFile_) {
        fileBuffer_.insert(fileBuffer_.end(), samples.begin(), samples.end());
    }

    queueCondition_.notify_one();
    return true;
}

void AudioPlayer::flush() {
    if (wavFile_ && !fileBuffer_.empty()) {
        wavFile_->write(reinterpret_cast<const char*>(fileBuffer_.data()), 
                       fileBuffer_.size() * sizeof(int16_t));
        totalSamplesWritten_ += fileBuffer_.size();
        fileBuffer_.clear();
    }
}

size_t AudioPlayer::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return audioQueue_.size();
}

int AudioPlayer::audioCallback(const void* inputBuffer, void* outputBuffer,
                              unsigned long framesPerBuffer,
                              const PaStreamCallbackTimeInfo* timeInfo,
                              PaStreamCallbackFlags statusFlags,
                              void* userData) {
    (void)inputBuffer;  // Unused
    (void)timeInfo;     // Unused
    (void)statusFlags;  // Unused

    AudioPlayer* player = static_cast<AudioPlayer*>(userData);
    int16_t* output = static_cast<int16_t*>(outputBuffer);

    int samplesProvided = player->fillAudioBuffer(output, framesPerBuffer);
    
    // Fill remaining buffer with silence if needed
    if (samplesProvided < static_cast<int>(framesPerBuffer)) {
        std::memset(output + samplesProvided, 0, 
                   (framesPerBuffer - samplesProvided) * sizeof(int16_t));
    }

    return paContinue;
}

int AudioPlayer::fillAudioBuffer(int16_t* output, unsigned long frameCount) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    
    unsigned long samplesProvided = 0;
    
    while (samplesProvided < frameCount && !audioQueue_.empty()) {
        output[samplesProvided] = audioQueue_.front();
        audioQueue_.pop();
        samplesProvided++;
    }
    
    return static_cast<int>(samplesProvided);
}

void AudioPlayer::initializeWavFile() {
    wavFile_ = std::make_unique<std::ofstream>(saveFile_, std::ios::binary);
    if (!wavFile_->is_open()) {
        std::cerr << "Failed to open WAV file: " << saveFile_ << std::endl;
        wavFile_.reset();
        return;
    }

    writeWavHeader();
    totalSamplesWritten_ = 0;
    std::cout << "Saving audio to: " << saveFile_ << std::endl;
}

void AudioPlayer::writeWavHeader() {
    if (!wavFile_) return;

    // WAV header structure
    struct WavHeader {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t fileSize = 0;  // Will be updated later
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4] = {'f', 'm', 't', ' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1;  // PCM
        uint16_t numChannels = 1;  // Mono
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign = 2;   // 16-bit mono
        uint16_t bitsPerSample = 16;
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize = 0;     // Will be updated later
    } header;

    header.sampleRate = sampleRate_;
    header.byteRate = sampleRate_ * 2;  // Sample rate * channels * bytes per sample

    wavFile_->write(reinterpret_cast<const char*>(&header), sizeof(header));
}

void AudioPlayer::finalizeWavFile() {
    if (!wavFile_) return;

    // Write any remaining buffer data
    flush();

    // Update WAV header with correct sizes
    uint32_t dataSize = totalSamplesWritten_ * sizeof(int16_t);
    uint32_t fileSize = dataSize + 36;  // Total file size - 8 bytes

    // Seek to file size field and update
    wavFile_->seekp(4);
    wavFile_->write(reinterpret_cast<const char*>(&fileSize), 4);

    // Seek to data size field and update
    wavFile_->seekp(40);
    wavFile_->write(reinterpret_cast<const char*>(&dataSize), 4);

    wavFile_->close();
    wavFile_.reset();

    std::cout << "WAV file finalized: " << totalSamplesWritten_ << " samples written" << std::endl;
}
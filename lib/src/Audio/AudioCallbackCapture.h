#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

class AudioCallbackCapture {
public:
    struct Result {
        bool completed {};
        double sampleRate {};
        uint64_t firstCallback {};
        uint64_t lastCallback {};
        float peak {};
        float rms {};
        std::vector<float> left;
        std::vector<float> right;
    };

    bool begin(double sampleRate, int durationMs);
    Result waitForCompletion(int timeoutMs);
    void append(
            float* const* outputChannelData,
            int outputChannelCount,
            int frameCount);
    void cancel();

    bool isCapturing() const;
    uint64_t callbackCount() const;

private:
    static constexpr size_t capacity = 65536;

    std::array<float, capacity> left;
    std::array<float, capacity> right;
    std::atomic<size_t> targetFrames {};
    std::atomic<size_t> position {};
    std::atomic<bool> completed {};
    std::atomic<uint64_t> callbacks {};
    std::atomic<uint64_t> firstCallback {};
    std::atomic<uint64_t> lastCallback {};
    double captureSampleRate {};
};

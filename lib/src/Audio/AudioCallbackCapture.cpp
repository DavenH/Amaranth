#include <cmath>

#include <Array/Buffer.h>
#include <JuceHeader.h>

#include "Audio/AudioCallbackCapture.h"

bool AudioCallbackCapture::begin(double sampleRate, int durationMs) {
    if (sampleRate <= 0.0 || durationMs <= 0 || isCapturing()) {
        return false;
    }

    captureSampleRate = sampleRate;
    const size_t requestedFrames = juce::jlimit(
            (size_t) 1,
            capacity,
            (size_t) juce::roundToInt(sampleRate * (double) durationMs / 1000.0));
    position.store(0, std::memory_order_relaxed);
    completed.store(false, std::memory_order_relaxed);
    firstCallback.store(0, std::memory_order_relaxed);
    lastCallback.store(0, std::memory_order_relaxed);
    targetFrames.store(requestedFrames, std::memory_order_release);
    return true;
}

AudioCallbackCapture::Result AudioCallbackCapture::waitForCompletion(int timeoutMs) {
    Result result;
    result.sampleRate = captureSampleRate;
    const uint32_t started = juce::Time::getMillisecondCounter();
    while (isCapturing()
            && juce::Time::getMillisecondCounter() - started < (uint32_t) juce::jmax(1, timeoutMs)) {
        juce::Thread::sleep(5);
    }

    const size_t capturedFrames = position.load(std::memory_order_acquire);
    result.completed = completed.load(std::memory_order_acquire);
    if (!result.completed) {
        cancel();
        return result;
    }

    result.firstCallback = firstCallback.load(std::memory_order_acquire);
    result.lastCallback = lastCallback.load(std::memory_order_acquire);
    result.left.assign(left.begin(), left.begin() + (int) capturedFrames);
    result.right.assign(right.begin(), right.begin() + (int) capturedFrames);

    Buffer<float> leftBuffer(result.left.data(), (int) result.left.size());
    Buffer<float> rightBuffer(result.right.data(), (int) result.right.size());
    const double leftNorm = leftBuffer.normL2();
    const double rightNorm = rightBuffer.normL2();
    const double squaredNorm = leftNorm * leftNorm + rightNorm * rightNorm;
    std::vector<float> absolute = result.left;
    Buffer<float> absoluteBuffer(absolute.data(), (int) absolute.size());
    absoluteBuffer.abs();
    result.peak = absoluteBuffer.max();
    absolute = result.right;
    absoluteBuffer = Buffer<float>(absolute.data(), (int) absolute.size());
    absoluteBuffer.abs();
    result.peak = juce::jmax(result.peak, absoluteBuffer.max());
    result.rms = (float) std::sqrt(squaredNorm / (double) (capturedFrames * 2));
    return result;
}

void AudioCallbackCapture::append(
        float* const* outputChannelData,
        int outputChannelCount,
        int frameCount) {
    const uint64_t callback = callbacks.fetch_add(1, std::memory_order_acq_rel) + 1;
    const size_t target = targetFrames.load(std::memory_order_acquire);
    if (target == 0 || frameCount <= 0) {
        return;
    }

    const size_t currentPosition = position.load(std::memory_order_relaxed);
    if (currentPosition >= target) {
        return;
    }
    if (currentPosition == 0) {
        firstCallback.store(callback, std::memory_order_relaxed);
    }

    const int copyCount = (int) juce::jmin(
            (size_t) frameCount,
            target - currentPosition);
    Buffer<float> capturedLeft(left.data() + currentPosition, copyCount);
    Buffer<float> capturedRight(right.data() + currentPosition, copyCount);
    if (outputChannelCount > 0 && outputChannelData[0] != nullptr) {
        Buffer<float>(outputChannelData[0], copyCount).copyTo(capturedLeft);
    } else {
        capturedLeft.zero();
    }
    if (outputChannelCount > 1 && outputChannelData[1] != nullptr) {
        Buffer<float>(outputChannelData[1], copyCount).copyTo(capturedRight);
    } else {
        capturedLeft.copyTo(capturedRight);
    }

    const size_t nextPosition = currentPosition + (size_t) copyCount;
    position.store(nextPosition, std::memory_order_release);
    if (nextPosition >= target) {
        lastCallback.store(callback, std::memory_order_relaxed);
        completed.store(true, std::memory_order_release);
        targetFrames.store(0, std::memory_order_release);
    }
}

void AudioCallbackCapture::cancel() {
    targetFrames.store(0, std::memory_order_release);
}

bool AudioCallbackCapture::isCapturing() const {
    return targetFrames.load(std::memory_order_acquire) != 0;
}

uint64_t AudioCallbackCapture::callbackCount() const {
    return callbacks.load(std::memory_order_acquire);
}

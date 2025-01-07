#pragma once
#include <JuceHeader.h>
#include <Array/ScopedAlloc.h>
#include <ipp.h>
#include <Array/RingBuffer.h>

class OscAudioProcessor : public AudioIODeviceCallback {
public:
    OscAudioProcessor();
    ~OscAudioProcessor() override;
    void start();
    void stop();
    void setTargetFrequency(float freq);
    void resetPeriods();
    float getTargetPeriod() const { return targetPeriod; }
    [[nodiscard]] double getCurrentSampleRate() const;
    std::vector<Buffer<Float32>> getAudioPeriods() const;

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(AudioIODevice* device) override;
    void audioDeviceStopped() override {
    }

    static constexpr int kBufferSize = 2048;

    std::unique_ptr<AudioDeviceManager> deviceManager;
    float accumulatedSamples;
    float targetPeriod { 44100.0f / 440.0f }; // Default to A4

    ScopedAlloc<Float32> workBuffer;
    ScopedAlloc<Float32> workBufferUI;
    ReadWriteBuffer rwBufferAudioThread;

    std::vector<Buffer<Float32> > periods;

    SpinLock bufferLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscAudioProcessor)
};

#pragma once
#include <JuceHeader.h>
#include <Array/ScopedAlloc.h>
#include <Array/RingBuffer.h>

class RealTimePitchTracker;
class TestableOscAudioProcessor;

class OscAudioProcessor : public AudioIODeviceCallback {
public:
    OscAudioProcessor(RealTimePitchTracker* pitchTracker);
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
    void audioDeviceStopped() override {}
    void appendSamplesRetractingPeriods(Buffer<float>& audioBlock);
    void applyDynamicRangeCompression(Buffer<float> audio);

    float accumulatedSamples{};
    static constexpr int kBufferSize = 2048;
    float targetPeriod { 44100.0f / 440.0f }; // Default to A4

    ScopedAlloc<Float32> workBuffer;
    ScopedAlloc<Float32> workBufferUI;
    ReadWriteBuffer rwBufferAudioThread;
    RealTimePitchTracker* pitchTracker;
    std::unique_ptr<AudioDeviceManager> deviceManager;
    std::vector<Buffer<Float32> > periods;

    SpinLock bufferLock;
    friend class TestableOscAudioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscAudioProcessor)
};

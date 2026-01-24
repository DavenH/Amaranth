#pragma once
#include <JuceHeader.h>
#include <Array/ScopedAlloc.h>
#include <Array/RingBuffer.h>
#include <array>

class RealTimePitchTracker;
class TestableOscAudioProcessor;

class OscAudioProcessor : public AudioIODeviceCallback {
public:
    struct OnsetEvent {
        double timeSeconds{};
        float strength{};
    };

    OscAudioProcessor(RealTimePitchTracker* pitchTracker);
    ~OscAudioProcessor() override;
    void start();
    void stop();
    void setTargetFrequency(float freq);
    void resetPeriods();
    float getTargetPeriod() const { return targetPeriod; }
    [[nodiscard]] double getCurrentSampleRate() const;
    std::vector<Buffer<Float32>> getAudioPeriods() const;
    void popOnsetEvents(std::vector<OnsetEvent>& out);

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
    void detectOnsets(Buffer<float>& audioBlock);

    float accumulatedSamples{};
    static constexpr int kBufferSize = 2048;
    float targetPeriod { 44100.0f / 440.0f }; // Default to A4
    double sampleRateHz { 44100.0 };
    int64 totalSamplesProcessed {};
    int64 samplesSinceLastOnset {};
    float onsetEnvelope {};
    float previousRms {};
    static constexpr float kOnsetEnvSmoothing = 0.08f;
    static constexpr float kOnsetThresholdRatio = 2.5f;
    static constexpr float kOnsetFluxThreshold = 0.01f;
    static constexpr float kOnsetMinRms = 0.01f;
    static constexpr double kOnsetMinIntervalSec = 0.12;
    static constexpr int kOnsetQueueSize = 32;
    AbstractFifo onsetFifo { kOnsetQueueSize };
    std::array<OnsetEvent, kOnsetQueueSize> onsetQueue;

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

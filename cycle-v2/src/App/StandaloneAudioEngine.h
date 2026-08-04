#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>
#include <vector>

#include "../Runtime/RealtimeGraphRenderer.h"

namespace CycleV2 {

class StandaloneAudioEngine final :
        public juce::AudioIODeviceCallback
    ,   public juce::MidiInputCallback
    ,   public MidiEventSink
    ,   private juce::Timer {
public:
    struct Status {
        bool deviceReady {};
        juce::String deviceName;
        juce::String error;
        double sampleRate {};
        int blockSize {};
        uint64_t preparationRevision {};
        RealtimeGraphRenderer::Diagnostics renderer;
    };

    struct LiveCapture {
        bool completed {};
        double sampleRate {};
        uint64_t firstCallback {};
        uint64_t lastCallback {};
        float peak {};
        float rms {};
        std::vector<float> left;
        std::vector<float> right;
    };

    StandaloneAudioEngine();
    ~StandaloneAudioEngine() override;

    bool start();
    void stop();
    bool publishGraph(GraphExecutionPlan plan, uint64_t revision);
    Status status() const;
    LiveCapture captureLiveAudio(int durationMs);

    bool enqueueMidiMessage(
            const juce::MidiMessage& message,
            MidiEventSource source) override;
    void releaseMidiSource(MidiEventSource source) override;

    void audioDeviceIOCallbackWithContext(
            const float* const* inputChannelData,
            int inputChannelCount,
            float* const* outputChannelData,
            int outputChannelCount,
            int frameCount,
            const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void handleIncomingMidiMessage(
            juce::MidiInput* source,
            const juce::MidiMessage& message) override;

private:
    using PreparedGraph = RealtimeGraphRenderer::PreparedGraph;

    void timerCallback() override;
    void adoptPendingGraph();
    void reclaimGraph(PreparedGraph* graph);
    void captureOutput(
            float* const* outputChannelData,
            int outputChannelCount,
            int frameCount);
    double currentTimeSeconds() const;

    std::unique_ptr<juce::PropertiesFile> deviceProperties;
    juce::AudioDeviceManager deviceManager;
    RealtimeMidiEventQueue midiEvents;
    RealtimeGraphRenderer renderer;
    std::vector<std::unique_ptr<PreparedGraph>> graphOwners;
    std::atomic<PreparedGraph*> pendingGraph {};
    std::atomic<PreparedGraph*> retiredGraph {};
    PreparedGraph* activeGraph {};

    std::atomic<bool> ready {};
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int> currentBlockSize { 512 };
    std::atomic<uint64_t> devicePreparationRevision { 1 };
    static constexpr size_t liveCaptureCapacity = 65536;
    std::array<float, liveCaptureCapacity> liveCaptureLeft;
    std::array<float, liveCaptureCapacity> liveCaptureRight;
    std::atomic<size_t> liveCaptureTarget {};
    std::atomic<size_t> liveCapturePosition {};
    std::atomic<uint64_t> liveCaptureFirstCallback {};
    std::atomic<uint64_t> liveCaptureLastCallback {};
    juce::String currentDeviceName;
    juce::String deviceError;
};

}

#pragma once
#include <JuceHeader.h>
#include <array>
#include <UI/Widgets/AmaranthMidiKeyboard.h>

#include "OscAudioProcessor.h"
#include <Algo/FFT.h>

#include "GradientColorMap.h"
#include "PhaseVelocityHistoryRenderer.h"
#include "RealTimePitchTracker.h"
#include "TempermentControls.h"

class HighlightKeyboard : public AmaranthMidiKeyboard {
public:
    using AmaranthMidiKeyboard::AmaranthMidiKeyboard;

    void setHighlightedNote(int note) {
        AmaranthMidiKeyboard::setHighlightedNote(note);
    }

    std::function<void(int)> onArrowKey;

    bool keyPressed(const KeyPress& key) override {
        if (key.getKeyCode() == KeyPress::rightKey) {
            if (onArrowKey) {
                onArrowKey(1);
            }
            return true;
        }
        if (key.getKeyCode() == KeyPress::leftKey) {
            if (onArrowKey) {
                onArrowKey(-1);
            }
            return true;
        }
        return false;
    }

};

class MainComponent : public Component,
                      public Timer,
                      public MidiKeyboardStateListener
                    , public RealTimePitchTraceListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(Graphics&) override;
    void drawPhaseVelocityBarChart(Graphics& g, const Rectangle<int>& area);
    void drawHarmonicPhaseVelocityPlot(Graphics& g, const Rectangle<int>& area, int harmonicIndex);
    void drawWeightedPhaseVelocityPlot(Graphics& g, const Rectangle<int>& area);
    void showTemperamentDialog();
    void stepRootNote(int delta);
    bool isRealtimePitchTrackingEnabled() const { return realtimePitchTrackingEnabled; }
    void setRealtimePitchTrackingEnabled(bool shouldEnable);
    RealTimePitchTracker::Algorithm getRealtimePitchTrackingAlgorithm() const;
    void setRealtimePitchTrackingAlgorithm(RealTimePitchTracker::Algorithm algorithm);

    void resized() override;
    void timerCallback() override;

    void updateCurrentNote();
    void handleNoteOn(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    // Pitch debug (disabled):
    void onPitchFrame(int bestKeyIndex,
        const Buffer<float>& windowedAudio,
        const Buffer<float>& kernelCorrelations,
        const Buffer<float>& fftMagnitudes) override;

private:
    void calculateTrueDrift();
    void handleOnsetEvents();
    void pushNoteHistory(int midiNoteNumber);
    void appendCurrentPhaseVelocity();
    void clearNoteHistories();
    void updatePitchTrackingState();

    // Pitch debug (disabled):
    void appendPitchDebugImage();

    static constexpr int kTopKHarmonics = 4;
    static constexpr int kImageHeight = 512;
    static constexpr int kNumPhasePartials = 8;
    static constexpr int kHistoryFrames = 512;
    static constexpr int kNoteHistoryCount = PhaseVelocityHistory::kNumTracks;
    static constexpr int kNoteHistoryFrames = 512;
    static constexpr int kNoteHistoryAdvancePerTick = 4;
    static constexpr int kSpectrogramHeight = 10;
    static constexpr int kPitchDebugHeight = 88;
    static constexpr int kNumColours = 64;
    static constexpr int kPhaseVelocityWidth = 20;
    static constexpr float kPhaseSmoothing = 0.04f; // Exponential smoothing factor (0 to 1)
    static constexpr float kPhaseTrackingMinMagnitude = 0.05f;
    static constexpr float kBarChartMaxVelocity = 0.1f; // Maximum velocity for scaling bars
    static constexpr float kPlotMaxVelocity = 0.05f;
    static constexpr int kPitchVoteWindowFrames = 5;
    static constexpr int kPitchVoteQuorum = 3;

    using NoteHistory = PhaseVelocityHistory;

    MidiKeyboardState keyboardState;
    std::array<bool, kNumPhasePartials> hasPreviousPhase{};
    std::array<bool, kNumPhasePartials> hasPhaseVelocitySeed{};
    bool realtimePitchTrackingEnabled = true;
    RealTimePitchTracker::Algorithm realtimePitchTrackingAlgorithm = RealTimePitchTracker::AlgoSpectral;

    std::unique_ptr<OscAudioProcessor> processor;
    std::unique_ptr<HighlightKeyboard> keyboard;
    std::unique_ptr<TemperamentControls> temperamentControls;
    std::unique_ptr<RealTimePitchTracker> pitchTracker;
    std::unique_ptr<DialogWindow> temperamentDialog;

    int lastClickedMidiNote = 60;
    float trueDrift = 0.0f;
    float trueDriftMagnitude = 0.0f;
    float keyboardKeyWidth = 18.0f;

    Image phaseVelocityBar;
    Image cyclogram, spectrogram, phasigram;
    Image pitchDebug;
    Rectangle<int> plotBounds;
    ScopedAlloc<Float32> resampleBuffer{kImageHeight};
    ScopedAlloc<Float32> workBuffer;
    ScopedAlloc<Float32> prevPhases{kNumPhasePartials};
    ScopedAlloc<Float32> phaseVelocity{kNumPhasePartials};
    ScopedAlloc<Float32> avgMagnitudes{kNumPhasePartials};
    Transform transform;
    std::array<NoteHistory, kNoteHistoryCount> noteHistories{};
    int currentNoteHistory = -1;
    int noteSequenceCounter = 0;

    GradientColourMap viridis, inferno;
    GradientColourMap bipolar;
    // Pitch debug (disabled):
    SpinLock pitchTraceLock;
    ScopedAlloc<float> latestKernelCorrelations{kPitchDebugHeight};
    bool hasPitchTraceFrame = false;

    int latestDetectedMidi = 69;
    std::array<int, 128> pitchVoteCounts{};
    bool pitchVoteActive = false;
    int pitchVoteFramesRemaining = 0;
    int pitchVoteBestMidi = 69;
    int pitchVoteBestCount = 0;

    void updateHistoryImage();
    void drawHistoryImage(Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

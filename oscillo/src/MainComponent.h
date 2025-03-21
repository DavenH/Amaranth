#pragma once
#include <JuceHeader.h>
#include "OscAudioProcessor.h"
#include <Algo/FFT.h>

#include "GradientColorMap.h"
#include "RealTimePitchTracker.h"
#include "TempermentControls.h"

class MainComponent : public Component,
                      public Timer,
                      public MidiKeyboardStateListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(Graphics&) override;
    void drawPhaseVelocityBarChart(Graphics& g, const Rectangle<int>& area);

    void resized() override;
    void timerCallback() override;

    void updateCurrentNote();
    void handleNoteOn(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

private:
    void calculateTrueDrift();

    static constexpr int kTopKHarmonics = 4;
    static constexpr int kImageHeight = 512;
    static constexpr int kNumPhasePartials = 8;
    static constexpr int kHistoryFrames = 512;
    static constexpr int kNumColours = 64;
    static constexpr int kPhaseVelocityWidth = 20;
    static constexpr float kPhaseSmoothing = 0.05f; // Exponential smoothing factor (0 to 1)
    static constexpr float kBarChartMaxVelocity = 0.1f; // Maximum velocity for scaling bars

    MidiKeyboardState keyboardState;

    std::unique_ptr<OscAudioProcessor> processor;
    std::unique_ptr<MidiKeyboardComponent> keyboard;
    std::unique_ptr<TemperamentControls> temperamentControls;
    std::unique_ptr<RealTimePitchTracker> pitchTracker;

    int lastClickedMidiNote = 60;
    float trueDrift = 0.0f;

    Image phaseVelocityBar;
    Image cyclogram, spectrogram, phasigram;
    Rectangle<int> plotBounds;
    ScopedAlloc<Float32> resampleBuffer{kImageHeight};
    ScopedAlloc<Float32> workBuffer;
    ScopedAlloc<Float32> prevPhases{kNumPhasePartials};
    ScopedAlloc<Float32> phaseDiff{kNumPhasePartials};
    ScopedAlloc<Float32> phaseVelocity{kNumPhasePartials};
    ScopedAlloc<Float32> avgMagnitudes{kNumPhasePartials};
    Transform transform;

    GradientColourMap viridis, inferno;
    GradientColourMap bipolar;

    void updateHistoryImage();
    void drawHistoryImage(Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

#pragma once
#include <JuceHeader.h>
#include "OscAudioProcessor.h"
#include <Algo/FFT.h>

#include "GradientColorMap.h"
#include "TempermentControls.h"

class MainComponent : public Component,
                      public Timer,
                      public MidiKeyboardStateListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void updateCurrentNote();
    void handleNoteOn(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

private:
    static constexpr int kImageHeight = 512;
    static constexpr int kNumPhasePartials = 16;
    static constexpr int kHistoryFrames = 512;
    static constexpr int kNumColours = 64;
    static constexpr int kPhaseVelocityWidth = 20;
    static constexpr float kPhaseSmoothing = 0.5f; // Exponential smoothing factor (0 to 1)

    OscAudioProcessor processor;
    MidiKeyboardState keyboardState;
    std::unique_ptr<MidiKeyboardComponent> keyboard;
    std::unique_ptr<TemperamentControls> temperamentControls;

    int lastClickedMidiNote = 60;
    Image phaseVelocityBar;
    Image cyclogram, spectrogram, phasigram;
    Rectangle<int> plotBounds;
    ScopedAlloc<Float32> resampleBuffer{kImageHeight};
    ScopedAlloc<Float32> workBuffer;
    ScopedAlloc<Float32> prevPhases{kNumPhasePartials};
    ScopedAlloc<Float32> phaseDiff{kNumPhasePartials};
    ScopedAlloc<Float32> phaseVelocity{kNumPhasePartials};
    Transform transform;

    GradientColourMap viridis, inferno;
    GradientColourMap bipolar;

    void updateHistoryImage();
    void drawHistoryImage(Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

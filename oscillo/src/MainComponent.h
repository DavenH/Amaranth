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
    static constexpr int kHistoryFrames = 512;
    static constexpr int kNumColours = 24;

    OscAudioProcessor processor;
    MidiKeyboardState keyboardState;
    std::unique_ptr<MidiKeyboardComponent> keyboard;
    std::unique_ptr<TemperamentControls> temperamentControls;

    int lastClickedMidiNote = 60;
    Image cyclogram, spectrogram;
    Rectangle<int> plotBounds;
    ScopedAlloc<Float32> resampleBuffer{kImageHeight};
    ScopedAlloc<Float32> workBuffer;
    Transform transform;

    GradientColourMap viridis, inferno;
    
    void updateHistoryImage();
    void drawHistoryImage(Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

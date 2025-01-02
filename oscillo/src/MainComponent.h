#pragma once
#include <JuceHeader.h>
#include "OscAudioProcessor.h"
#include <Algo/FFT.h>

#include "GradientColorMap.h"

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

    void handleNoteOn(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

private:
    static constexpr int kImageHeight = 512;
    static constexpr int kHistoryFrames = 512;
    static constexpr int kNumColours = 24;

    OscAudioProcessor processor;
    MidiKeyboardState keyboardState;
    std::unique_ptr<MidiKeyboardComponent> keyboard;
    
    Image cyclogram, spectrogram;
    Rectangle<int> plotBounds;
    ScopedAlloc<Ipp32f> resampleBuffer{kImageHeight};
    ScopedAlloc<Ipp32f> workBuffer;
    Transform transform;

    GradientColourMap viridis, inferno;
    
    void updateHistoryImage();
    void drawHistoryImage(Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

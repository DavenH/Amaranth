#pragma once
#include <JuceHeader.h>
#include "OscAudioProcessor.h"

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

    // MidiKeyboardStateListener implementation
    void handleNoteOn(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

private:
    static constexpr int kImageHeight = 512;
    static constexpr int kHistoryFrames = 512;

    OscAudioProcessor processor;
    MidiKeyboardState keyboardState;
    std::unique_ptr<MidiKeyboardComponent> keyboard;
    
    Image historyImage;
    Rectangle<int> plotBounds;
    ScopedAlloc<Ipp32f> resampleBuffer{kImageHeight};
    ScopedAlloc<Ipp32f> workBuffer;
    
    void updateHistoryImage();
    void drawHistoryImage(Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
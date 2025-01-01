#pragma once
#include <JuceHeader.h>
#include "OscAudioProcessor.h"
#include "PianoComponent.h"

class MainComponent : public Component,
                     public Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    static constexpr int kImageHeight = 512;
    static constexpr int kHistoryFrames = 512;

    OscAudioProcessor processor;
    std::unique_ptr<PianoComponent> keyboard;
    
    Image historyImage;
    Rectangle<int> plotBounds;
    ScopedAlloc<Ipp32f> resampleBuffer{kImageHeight};
    
    void updateHistoryImage();
    void drawHistoryImage(Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
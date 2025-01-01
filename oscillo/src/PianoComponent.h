#pragma once
#include <JuceHeader.h>
#include <functional>

struct Note {
    const char* name;
    float baseFrequency;
    bool isBlack;
};

class PianoKey : public juce::TextButton {
public:
    PianoKey(const Note& note, int octave);

    float getFrequency() const { return frequency; }

private:
    float frequency;
};

class PianoComponent
        : public juce::Component,
          public juce::ComboBox::Listener {
public:
    explicit PianoComponent(std::function<void(float)> callback);

    ~PianoComponent() override;

    void resized() override;

    void comboBoxChanged(juce::ComboBox*) override;

private:
    static constexpr int kDefaultOctave = 4;

    std::function<void(float)> frequencyCallback;
    std::unique_ptr<juce::ComboBox> octaveSelector;
    std::vector<std::unique_ptr<PianoKey> > keys;

    void createKeys();

    void updateFrequencies();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoComponent)
};

#include "PianoComponent.h"

namespace {
    const std::array<Note, 12> kNotes = {
        {
            { "C", 261.63f, false },
            { "C#", 277.18f, true },
            { "D", 293.66f, false },
            { "D#", 311.13f, true },
            { "E", 329.63f, false },
            { "F", 349.23f, false },
            { "F#", 369.99f, true },
            { "G", 392.00f, false },
            { "G#", 415.30f, true },
            { "A", 440.00f, false },
            { "A#", 466.16f, true },
            { "B", 493.88f, false }
        }
    };
}

PianoKey::PianoKey(const Note& note, int octave)
    : TextButton(note.name + juce::String(octave)),
      frequency(note.baseFrequency * std::pow(2.0f, static_cast<float>(octave - 4))) {
    setClickingTogglesState(false);
    setColour(buttonColourId, note.isBlack
                                  ? juce::Colours::black
                                  : juce::Colours::white);
    setColour(textColourOffId, note.isBlack
                                   ? juce::Colours::white
                                   : juce::Colours::black);
}

PianoComponent::PianoComponent(std::function<void(float)> callback)
    : frequencyCallback(std::move(callback)) {
    octaveSelector = std::make_unique<juce::ComboBox>();
    octaveSelector->addItemList({ "0", "1", "2", "3", "4", "5", "6" }, 1);
    octaveSelector->setSelectedId(kDefaultOctave + 1);
    octaveSelector->addListener(this);
    addAndMakeVisible(octaveSelector.get());

    createKeys();
}

PianoComponent::~PianoComponent() {
    octaveSelector->removeListener(this);
}

void PianoComponent::createKeys() {
    auto octave = octaveSelector->getSelectedId() - 1;

    for (const auto& note: kNotes) {
        auto key     = std::make_unique<PianoKey>(note, octave);
        key->onClick = [this, key = key.get()]() {
            frequencyCallback(key->getFrequency());
        };

        addAndMakeVisible(key.get());
        keys.push_back(std::move(key));
    }
}

void PianoComponent::resized() {
    auto area = getLocalBounds();

    auto topStrip = area.removeFromTop(30);
    octaveSelector->setBounds(topStrip.withWidth(80));

    auto keyWidth     = area.getWidth() / 7;
    int blackKeyWidth = keyWidth * 2 / 3;

    int whiteKeyIndex = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        auto* key    = keys[i].get();
        bool isBlack = kNotes[i].isBlack;

        if (!isBlack) {
            key->setBounds(area.getX() + whiteKeyIndex * keyWidth,
                           area.getY(),
                           keyWidth - 1,
                           area.getHeight());
            whiteKeyIndex++;
        } else {
            key->setBounds(area.getX() + (whiteKeyIndex - 0.5f) * keyWidth,
                           area.getY(),
                           blackKeyWidth,
                           area.getHeight() * 0.6f);
        }
    }
}

void PianoComponent::comboBoxChanged(juce::ComboBox*) {
    keys.clear();
    createKeys();
    resized();
}

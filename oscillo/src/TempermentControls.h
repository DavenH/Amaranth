#pragma once
#include <JuceHeader.h>

class TemperamentControls
        : public Component,
          public ComboBox::Listener,
          public Slider::Listener {
public:
    enum class Temperament {
        Equal = 1,
        Railsback = 2
    };

    TemperamentControls() {
        temperamentSelect.addItem("Equal Temperament", (int) Temperament::Equal);
        temperamentSelect.addItem("Railsback Curve", (int) Temperament::Railsback);
        temperamentSelect.setSelectedId((int) (Temperament::Equal));
        temperamentSelect.addListener(this);
        addAndMakeVisible(temperamentSelect);

        centsOffsetSlider.setRange(-50.0, 50.0, 0.1);
        centsOffsetSlider.setValue(0.0);
        centsOffsetSlider.setTextBoxStyle(Slider::TextBoxRight, false, 60, 20);
        centsOffsetSlider.addListener(this);
        addAndMakeVisible(centsOffsetSlider);

        temperamentLabel.setText("Temperament:", dontSendNotification);
        temperamentLabel.attachToComponent(&temperamentSelect, true);
        addAndMakeVisible(temperamentLabel);

        centsLabel.setText("Cents Offset:", dontSendNotification);
        centsLabel.attachToComponent(&centsOffsetSlider, true);
        addAndMakeVisible(centsLabel);

        frequencyLabel.setJustificationType(Justification::centred);
        addAndMakeVisible(frequencyLabel);

        // for(int i = 0; i < 128; ++i) {
        //     std::cout << MidiMessage::getMidiNoteName(i, true, true, 4) << " " << getCentsOffset(i, Temperament::Railsback) << std::endl;
        // }
    }

    void resized() override {
        auto area = getLocalBounds();
        auto column = area.removeFromRight(250);
        temperamentSelect.setBounds(column.removeFromTop(30));
        column.removeFromTop(10);
        centsOffsetSlider.setBounds(column.removeFromTop(30));
        column = area.removeFromRight(400);
        frequencyLabel.setBounds(column.removeFromTop(30));
    }

    [[nodiscard]] double getCentsOffset(int midiNote, Temperament selectedTemperment) const {
        double centsOffset = centsOffsetSlider.getValue();

        if (selectedTemperment == Temperament::Railsback) {
            double x = 2. * (midiNote - 65) / 75;
            double railsbackCents = 22 * x * x * x;
            centsOffset += railsbackCents;
        }

        return centsOffset;
    }

    [[nodiscard]] double getFrequencyMultiplier(int midiNote) const {
        return std::pow(2.0, getCentsOffset(midiNote, (Temperament) temperamentSelect.getSelectedId()) / 1200.0);
    }

    void setCurrentNote(int midiNote) {
        if (midiNote < 0) {
            frequencyLabel.setText("", dontSendNotification);
            return;
        }

        double baseFreq = MidiMessage::getMidiNoteInHertz(midiNote);
        double adjustedFreq = baseFreq * getFrequencyMultiplier(midiNote);
        double cents = getCentsOffset(midiNote, (Temperament)temperamentSelect.getSelectedId());

        String noteInfo = String::formatted("%s: %.1f Hz (%.1f cents)",
            MidiMessage::getMidiNoteName(midiNote, true, true, 4).toRawUTF8(),
            adjustedFreq,
            cents);

        frequencyLabel.setText(noteInfo, dontSendNotification);
    }

    void comboBoxChanged(ComboBox*) override {
        if (onTemperamentChanged) {
            onTemperamentChanged();
        }
    }

    void sliderDragEnded(Slider*) override {
        if (onCentsOffsetChanged) {
            onCentsOffsetChanged();
        }
    }

    void sliderValueChanged(Slider*) override {}

    std::function<void()> onTemperamentChanged;
    std::function<void()> onCentsOffsetChanged;

private:
    ComboBox temperamentSelect;
    Slider centsOffsetSlider;
    Label temperamentLabel;
    Label centsLabel;
    Label frequencyLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TemperamentControls)
};

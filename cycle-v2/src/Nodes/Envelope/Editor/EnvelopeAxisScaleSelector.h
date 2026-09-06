#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

namespace CycleV2 {

class EnvelopeAxisScaleSelector final : public juce::Component {
public:
    EnvelopeAxisScaleSelector();
    ~EnvelopeAxisScaleSelector() override;

    void setLogarithmic(
            bool logarithmic,
            juce::NotificationType notification = juce::dontSendNotification);
    bool isLogarithmic() const { return logarithmic; }
    juce::Rectangle<float> optionBounds(bool logarithmicOption) const;
    juce::Rectangle<float> optionDiagramBounds(bool logarithmicOption) const;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

    std::function<void(bool)> onChange;

private:
    class ScaleButton;

    bool logarithmic {};
    std::unique_ptr<ScaleButton> linearButton;
    std::unique_ptr<ScaleButton> logarithmicButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeAxisScaleSelector)
};

}

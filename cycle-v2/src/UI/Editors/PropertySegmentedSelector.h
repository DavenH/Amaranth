#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

namespace CycleV2 {

struct PropertySegmentOption {
    juce::String label;
    juce::String value;
    juce::String componentId;
    juce::String tooltip;
};

class PropertySegmentedSelector final : public juce::Component {
public:
    explicit PropertySegmentedSelector(std::vector<PropertySegmentOption> options);
    ~PropertySegmentedSelector() override;

    void setSelectedValue(
            const juce::String& value,
            juce::NotificationType notification = juce::dontSendNotification);
    const juce::String& selectedValue() const { return value; }
    int selectedIndex() const;
    juce::Rectangle<float> optionBounds(int index) const;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

    std::function<void(const juce::String&)> onChange;

private:
    class OptionButton;

    void select(int index, juce::NotificationType notification);
    void focus(int index);

    std::vector<PropertySegmentOption> optionDefinitions;
    std::vector<std::unique_ptr<OptionButton>> buttons;
    juce::String value;
};

}

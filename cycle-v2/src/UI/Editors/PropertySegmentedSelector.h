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
    std::vector<juce::Colour> ticks;
};

class PropertySegmentedSelector final : public juce::Component, private juce::Timer {
public:
    explicit PropertySegmentedSelector(std::vector<PropertySegmentOption> options);
    ~PropertySegmentedSelector() override;

    void setSelectedValue(
            const juce::String& value,
            juce::NotificationType notification = juce::dontSendNotification);
    const juce::String& selectedValue() const { return value; }
    int selectedIndex() const;
    juce::Rectangle<float> optionBounds(int index) const;
    void setHoverMessageProvider(std::function<juce::String(const juce::String&)> provider);

    void paint(juce::Graphics& graphics) override;
    void resized() override;

    std::function<void(const juce::String&)> onChange;

private:
    class OptionButton;

    void select(int index, juce::NotificationType notification);
    void focus(int index);
    void timerCallback() override;

    std::vector<PropertySegmentOption> optionDefinitions;
    std::vector<std::unique_ptr<OptionButton>> buttons;
    juce::String value;
    std::function<juce::String(const juce::String&)> hoverMessageProvider;
    std::unique_ptr<juce::BubbleMessageComponent> hoverCallout;
    int hoveredIndex { -1 };
    uint32_t hoverStarted {};
};

}

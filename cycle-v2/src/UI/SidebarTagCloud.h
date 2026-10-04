#pragma once

#include <JuceHeader.h>

#include <functional>
#include <vector>

namespace CycleV2 {

class SidebarTagCloud final : public juce::Component {
public:
    using ChangeCallback = std::function<void()>;

    static void styleHeading(juce::Label& heading);
    void setTags(juce::StringArray tags);
    const juce::StringArray& selectedTags() const { return selected; }
    bool matches(const juce::StringArray& recordTags) const;
    int preferredHeightForWidth(int width) const;
    void setChangeCallback(ChangeCallback callback);
    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation() const;

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    struct Chip {
        juce::String tag;
        juce::Rectangle<int> bounds;
    };

    std::vector<Chip> layoutForWidth(int width) const;

    juce::StringArray available;
    juce::StringArray selected;
    std::vector<Chip> chips;
    ChangeCallback onChange;
    int hovered { -1 };
};

}

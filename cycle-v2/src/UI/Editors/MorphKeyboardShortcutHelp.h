#pragma once

#include <JuceHeader.h>

#include <memory>

namespace CycleV2 {

class MorphKeyboardShortcutHelp : private juce::Timer {
public:
    MorphKeyboardShortcutHelp(
            juce::Component& host,
            const juce::String& componentId,
            bool includesYellowAxis);
    ~MorphKeyboardShortcutHelp();

    void setAnchorBounds(juce::Rectangle<float> focusOutlineBounds);
    void hide();

private:
    class Bubble;
    class Target;

    void showBubble();
    void hideBubble();
    void layoutBubble();
    void timerCallback() override;

    juce::Rectangle<float> iconBounds;
    std::unique_ptr<Bubble> bubble;
    std::unique_ptr<Target> target;
};

}

#pragma once

#include <JuceHeader.h>

#include <memory>

namespace CycleV2 {

class MorphKeyboardShortcutHelp {
public:
    MorphKeyboardShortcutHelp(
            juce::Component& host,
            const juce::String& componentId,
            bool includesYellowAxis);
    ~MorphKeyboardShortcutHelp();

    void setAnchorBounds(juce::Rectangle<float> focusOutlineBounds);
    void hide();

private:
    class Tooltip;
    class Target;

    std::unique_ptr<Target> target;
    std::unique_ptr<Tooltip> tooltip;
};

}

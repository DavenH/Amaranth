#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

enum class LibraryIcon {
    Curves, Presets, Patterns, Nodes, Assets,
    Import, AddReference, Tray, New, Edit, Rename, Delete,
    Count
};

class LibraryIconRenderer {
public:
    static void configure(juce::Button& button, LibraryIcon icon,
            const juce::String& tooltip);
    static bool hasIcon(LibraryIcon icon);
    static void paint(juce::Graphics& graphics, LibraryIcon icon,
            juce::Rectangle<float> canvas, bool highlighted);
    static bool paintButton(juce::Graphics& graphics, juce::Button& button);
};

}

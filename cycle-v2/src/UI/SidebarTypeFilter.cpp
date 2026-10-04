#include "UI/SidebarTypeFilter.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2::SidebarTypeFilter {

void configure(juce::ComboBox& comboBox) {
    comboBox.addItem("All types", 1);
    for (const auto* type : { "Bass", "Lead", "Pad", "Keys", "Sustained", "Rhythm", "Other" }) {
        comboBox.addItem(type, comboBox.getNumItems() + 2);
    }
    comboBox.setSelectedId(1, juce::dontSendNotification);
    comboBox.setColour(juce::ComboBox::backgroundColourId,
            CanvasChromePalette::restingControlSurface);
    comboBox.setColour(juce::ComboBox::textColourId, CanvasChromePalette::text);
    comboBox.setColour(juce::ComboBox::outlineColourId, CanvasChromePalette::border);
}

juce::String selectedType(const juce::ComboBox& comboBox) {
    return comboBox.getSelectedId() > 1 ? comboBox.getText() : juce::String();
}

juce::String canonicalType(const juce::String& type) {
    for (const auto* known : { "Bass", "Lead", "Pad", "Keys", "Sustained", "Rhythm" }) {
        if (type.equalsIgnoreCase(known)) {
            return known;
        }
    }
    return {};
}

}

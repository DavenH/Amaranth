#pragma once

#include <JuceHeader.h>

namespace CycleV2::SidebarTypeFilter {

void configure(juce::ComboBox& comboBox);
juce::String selectedType(const juce::ComboBox& comboBox);
juce::String canonicalType(const juce::String& type);

}

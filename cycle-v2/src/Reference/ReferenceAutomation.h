#pragma once

#include "Reference/ReferenceLibrary.h"

namespace CycleV2::ReferenceAutomation {

juce::var catalogue(const ReferenceLibrary& library);
juce::var selectedAnalysis(const ReferenceLibrary& library, bool includeGrid);

}

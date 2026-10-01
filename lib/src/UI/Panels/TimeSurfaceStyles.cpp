#include "TimeSurfaceStyles.h"

const TimeSurfaceStyles::Entry* TimeSurfaceStyles::find(int index) {
    for (const auto& entry : entries) {
        if ((int) entry.style == index) {
            return &entry;
        }
    }
    return nullptr;
}

const char* TimeSurfaceStyles::id(ScalarSurfaceTimeStyle style) {
    const auto* entry = find((int) style);
    return entry != nullptr ? entry->id : "blues";
}

ScalarSurfaceTimeStyle TimeSurfaceStyles::fromId(const juce::String& id) {
    for (const auto& entry : entries) {
        if (id == entry.id) {
            return entry.style;
        }
    }
    return ScalarSurfaceTimeStyle::BlueDepth;
}

juce::PopupMenu TimeSurfaceStyles::menu(ScalarSurfaceTimeStyle selected, int firstItemId) {
    juce::PopupMenu canonical;
    juce::PopupMenu experimental;
    juce::PopupMenu legacy;
    for (const auto& entry : entries) {
        auto& target = entry.group == Group::Canonical ? canonical
                : entry.group == Group::Experimental ? experimental : legacy;
        target.addItem(firstItemId + (int) entry.style, entry.label, true, selected == entry.style);
    }
    canonical.addSeparator();
    canonical.addSubMenu("Experimental", experimental);
    canonical.addSubMenu("Legacy", legacy);
    return canonical;
}

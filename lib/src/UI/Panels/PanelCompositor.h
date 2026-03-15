#pragma once

#include <vector>

#include "JuceHeader.h"
#include "PanelDirtyState.h"

class Panel;

class PanelCompositor {
public:
    struct Entry {
        Panel* panel;
        juce::Rectangle<int> bounds;
        uint32_t dirtyMask;
        int panelId;
        bool usesCachedSurface;
        bool visible;
    };

    void clear();
    void invalidateAll(PanelDirtyState::Flag flag = PanelDirtyState::Flag::Full);
    void invalidatePanel(Panel* panel, PanelDirtyState::Flag flag);
    void registerOrUpdatePanel(Panel* panel, const juce::Rectangle<int>& bounds, bool visible);
    void removePanel(Panel* panel);

    const std::vector<Entry>& getEntries() const { return entries; }
    std::vector<Entry> getVisibleEntries() const;

private:
    Entry* findEntry(Panel* panel);
    const Entry* findEntry(Panel* panel) const;

    std::vector<Entry> entries;
};

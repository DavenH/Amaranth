#include "PanelCompositor.h"

#include "Panel.h"

void PanelCompositor::clear() {
    entries.clear();
}

void PanelCompositor::invalidateAll(PanelDirtyState::Flag flag) {
    uint32_t mask = static_cast<uint32_t>(flag);

    for (auto& entry : entries) {
        entry.dirtyMask |= mask;
    }
}

void PanelCompositor::invalidatePanel(Panel* panel, PanelDirtyState::Flag flag) {
    if (Entry* entry = findEntry(panel)) {
        entry->dirtyMask |= static_cast<uint32_t>(flag);
    }
}

void PanelCompositor::registerOrUpdatePanel(Panel* panel, const juce::Rectangle<int>& bounds, bool visible) {
    if (panel == nullptr) {
        return;
    }

    if (Entry* entry = findEntry(panel)) {
        entry->bounds = bounds;
        entry->visible = visible;
        entry->usesCachedSurface = panel->usesCachedSurface();
        entry->dirtyMask |= panel->getDirtyState().mask();
        return;
    }

    entries.push_back({
        panel,
        bounds,
        panel->getDirtyState().mask(),
        panel->getPanelId(),
        panel->usesCachedSurface(),
        visible
    });
}

void PanelCompositor::removePanel(Panel* panel) {
    entries.erase(
        std::remove_if(entries.begin(), entries.end(), [panel](const Entry& entry) {
            return entry.panel == panel;
        }),
        entries.end()
    );
}

std::vector<PanelCompositor::Entry> PanelCompositor::getVisibleEntries() const {
    std::vector<Entry> visibleEntries;

    for (const auto& entry : entries) {
        if (entry.visible) {
            visibleEntries.push_back(entry);
        }
    }

    return visibleEntries;
}

PanelCompositor::Entry* PanelCompositor::findEntry(Panel* panel) {
    for (auto& entry : entries) {
        if (entry.panel == panel) {
            return &entry;
        }
    }

    return nullptr;
}

const PanelCompositor::Entry* PanelCompositor::findEntry(Panel* panel) const {
    for (const auto& entry : entries) {
        if (entry.panel == panel) {
            return &entry;
        }
    }

    return nullptr;
}

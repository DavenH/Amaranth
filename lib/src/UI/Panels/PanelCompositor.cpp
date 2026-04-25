#include "PanelCompositor.h"

#include "Panel.h"

void PanelCompositor::clear() {
    entries.clear();
    orphanedDirtyBounds.clear();
}

void PanelCompositor::clearDirtyFlags() {
    for (auto& entry : entries) {
        entry.dirtyMask = 0;
    }

    orphanedDirtyBounds.clear();
}

juce::RectangleList<int> PanelCompositor::collectDirtyBounds() const {
    juce::RectangleList<int> dirtyBounds(orphanedDirtyBounds);

    for (const auto& entry : entries) {
        if (entry.visible && entry.dirtyMask != 0) {
            dirtyBounds.add(entry.bounds);
        }
    }

    return dirtyBounds;
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
        bool boundsChanged = entry->bounds != bounds;
        bool visibilityChanged = entry->visible != visible;
        juce::Rectangle<int> oldBounds = entry->bounds;

        entry->bounds = bounds;
        entry->visible = visible;
        entry->usesCachedSurface = panel->usesCachedSurface();
        entry->dirtyMask |= panel->getDirtyState().mask();

        if (boundsChanged || visibilityChanged) {
            entry->dirtyMask |= static_cast<uint32_t>(PanelDirtyState::Flag::Layout);
            orphanedDirtyBounds.add(oldBounds);
        }

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
    if (const Entry* entry = findEntry(panel)) {
        orphanedDirtyBounds.add(entry->bounds);
    }

    entries.erase(
        std::remove_if(entries.begin(), entries.end(), [panel](const Entry& entry) {
            return entry.panel == panel;
        }),
        entries.end()
    );
}

void PanelCompositor::syncWithPanels() {
    for (auto& entry : entries) {
        if (entry.panel == nullptr) {
            continue;
        }

        entry.dirtyMask |= entry.panel->getDirtyState().mask();

        Component* component = entry.panel->getComponent();
        juce::Rectangle<int> bounds;
        bool visible = false;

        if (component != nullptr) {
            bounds = component->getBounds();
            visible = component->isVisible();
        }

        if (entry.bounds != bounds || entry.visible != visible) {
            orphanedDirtyBounds.add(entry.bounds);
            entry.bounds = bounds;
            entry.visible = visible;
            entry.dirtyMask |= static_cast<uint32_t>(PanelDirtyState::Flag::Layout);
        }
    }
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

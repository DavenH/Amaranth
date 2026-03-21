#include "SharedPanelCanvas.h"

#include "Panel.h"

SharedPanelCanvas::SharedPanelCanvas() {
    setInterceptsMouseClicks(false, false);
    setOpaque(false);
    startTimerHz(30);
}

void SharedPanelCanvas::invalidateAll(PanelDirtyState::Flag flag) {
    compositor.invalidateAll(flag);
    repaintDirtyRegions();
}

void SharedPanelCanvas::invalidatePanel(Panel* panel, PanelDirtyState::Flag flag) {
    compositor.invalidatePanel(panel, flag);
    repaintDirtyRegions();
}

void SharedPanelCanvas::registerOrUpdatePanel(Panel* panel, const juce::Rectangle<int>& bounds, bool visible) {
    compositor.registerOrUpdatePanel(panel, bounds, visible);
    repaintDirtyRegions();
}

void SharedPanelCanvas::removePanel(Panel* panel) {
    compositor.removePanel(panel);
    repaint();
}

void SharedPanelCanvas::setDebugSnapshotOverlayEnabled(bool enabled) {
    if (debugSnapshotOverlayEnabled == enabled) {
        return;
    }

    debugSnapshotOverlayEnabled = enabled;
    repaint();
}

void SharedPanelCanvas::paint(juce::Graphics& g) {
    if (debugSnapshotOverlayEnabled) {
        for (const auto& entry : compositor.getVisibleEntries()) {
            if (entry.panel == nullptr || !entry.usesCachedSurface || !g.getClipBounds().intersects(entry.bounds)) {
                continue;
            }

            entry.panel->paintSharedCanvasDebugOverlay(g, entry.bounds);
        }

        compositor.clearDirtyFlags();
        return;
    }

    for (const auto& entry : compositor.getVisibleEntries()) {
        if (entry.panel == nullptr || !g.getClipBounds().intersects(entry.bounds)) {
            continue;
        }

        if (entry.panel->usesSharedCanvasBackground()) {
            entry.panel->paintSharedCanvasBackground(g, entry.bounds);
        }

        if (entry.usesCachedSurface && entry.panel->usesSharedCanvasSurface()) {
            entry.panel->paintSharedCanvasSurface(g, entry.bounds);
        }
    }

    compositor.clearDirtyFlags();
}

void SharedPanelCanvas::repaintDirtyRegions() {
    juce::RectangleList<int> dirtyBounds = compositor.collectDirtyBounds();

    if (dirtyBounds.isEmpty()) {
        return;
    }

    for (int i = 0; i < dirtyBounds.getNumRectangles(); ++i) {
        repaint(dirtyBounds.getRectangle(i));
    }
}

void SharedPanelCanvas::timerCallback() {
    compositor.syncWithPanels();
    repaintDirtyRegions();
}

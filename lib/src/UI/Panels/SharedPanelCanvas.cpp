#include "SharedPanelCanvas.h"

#include "Panel.h"

void SharedPanelCanvas::invalidateAll(PanelDirtyState::Flag flag) {
    compositor.invalidateAll(flag);
    repaint();
}

void SharedPanelCanvas::invalidatePanel(Panel* panel, PanelDirtyState::Flag flag) {
    compositor.invalidatePanel(panel, flag);
    repaint();
}

void SharedPanelCanvas::registerOrUpdatePanel(Panel* panel, const juce::Rectangle<int>& bounds, bool visible) {
    compositor.registerOrUpdatePanel(panel, bounds, visible);
}

void SharedPanelCanvas::removePanel(Panel* panel) {
    compositor.removePanel(panel);
}

void SharedPanelCanvas::paint(juce::Graphics& g) {
    ignoreUnused(g);
}

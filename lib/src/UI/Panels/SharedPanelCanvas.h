#pragma once

#include "JuceHeader.h"
#include "PanelCompositor.h"

class Panel;

class SharedPanelCanvas :
        public juce::Component
    ,   private juce::Timer {
public:
    SharedPanelCanvas();
    ~SharedPanelCanvas() override = default;

    PanelCompositor& getCompositor() { return compositor; }
    const PanelCompositor& getCompositor() const { return compositor; }

    void invalidateAll(PanelDirtyState::Flag flag = PanelDirtyState::Flag::Full);
    void invalidatePanel(Panel* panel, PanelDirtyState::Flag flag);
    void registerOrUpdatePanel(Panel* panel, const juce::Rectangle<int>& bounds, bool visible);
    void removePanel(Panel* panel);
    void setDebugSnapshotOverlayEnabled(bool enabled);
    bool isDebugSnapshotOverlayEnabled() const { return debugSnapshotOverlayEnabled; }

    void paint(juce::Graphics& g) override;

private:
    void repaintDirtyRegions();
    void timerCallback() override;

    PanelCompositor compositor;
    bool debugSnapshotOverlayEnabled = false;
};

#pragma once

#include "JuceHeader.h"
#include "PanelCompositor.h"

class Panel;

class SharedPanelCanvas :
        public juce::Component {
public:
    SharedPanelCanvas() = default;
    ~SharedPanelCanvas() override = default;

    PanelCompositor& getCompositor() { return compositor; }
    const PanelCompositor& getCompositor() const { return compositor; }

    void invalidateAll(PanelDirtyState::Flag flag = PanelDirtyState::Flag::Full);
    void invalidatePanel(Panel* panel, PanelDirtyState::Flag flag);
    void registerOrUpdatePanel(Panel* panel, const juce::Rectangle<int>& bounds, bool visible);
    void removePanel(Panel* panel);

    void paint(juce::Graphics& g) override;

private:
    PanelCompositor compositor;
};

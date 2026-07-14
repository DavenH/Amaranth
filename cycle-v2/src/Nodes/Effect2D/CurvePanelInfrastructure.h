#pragma once

#include "../Trimesh/TrimeshPanelEnvironment.h"

#include <JuceHeader.h>

namespace CycleV2 {

class CurvePanelEnvironment {
public:
    TrimeshPanelEnvironment& services() { return environment; }

private:
    TrimeshPanelEnvironment environment;
};

class CurvePanelSnapshotCache {
public:
    void publish(juce::Image image, bool hasVisibleContent);
    bool paint(juce::Graphics& graphics, juce::Rectangle<float> bounds, bool resample) const;

private:
    mutable juce::CriticalSection lock;
    juce::Image image;
    bool visibleContent {};
};

class CurvePanelInteractionAdapter {
public:
    virtual ~CurvePanelInteractionAdapter() = default;
    virtual void beginEdit() = 0;
    virtual void publishIntermediateRevision() = 0;
    virtual void commitEdit() = 0;
};

}

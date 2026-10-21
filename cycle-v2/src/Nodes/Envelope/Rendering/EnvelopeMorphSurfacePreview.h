#pragma once

#include <functional>
#include <memory>

#include "JuceHeader.h"
#include <Obj/MorphPosition.h>

class GuideCurveProvider;
class Mesh;

namespace CycleV2 {

class TrimeshPanelEnvironment;

class EnvelopeMorphSurfacePreview {
public:
    EnvelopeMorphSurfacePreview(TrimeshPanelEnvironment& environment, std::function<void()> repaint);
    ~EnvelopeMorphSurfacePreview();

    juce::Component* component();
    void prepare(Mesh& mesh, GuideCurveProvider* guides, uint64_t revision,
            const MorphPosition& morph, int axis, int dimensions, bool gestureActive);
    void render(juce::Rectangle<float> bounds, float scaleFactor);
    void releaseOpenGLResources();
    juce::var automationState() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}

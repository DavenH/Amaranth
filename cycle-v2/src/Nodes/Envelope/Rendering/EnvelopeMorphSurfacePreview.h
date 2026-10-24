#pragma once

#include <functional>
#include <memory>

#include "JuceHeader.h"
#include <Obj/MorphPosition.h>

class Mesh;

namespace CycleV2 {

class TrimeshPanelEnvironment;
class GuideCurveSnapshotProvider;
struct PlaybackCursorFrame;

class EnvelopeMorphSurfacePreview {
public:
    EnvelopeMorphSurfacePreview(TrimeshPanelEnvironment& environment, std::function<void()> repaint);
    ~EnvelopeMorphSurfacePreview();

    juce::Component* component();
    void prepare(Mesh& mesh, GuideCurveSnapshotProvider* guides, uint64_t revision,
            const MorphPosition& morph, int axis, int dimensions, bool gestureActive,
            juce::Range<float> timeRange = { 0.f, 1.f });
    void render(juce::Rectangle<float> bounds, float scaleFactor);
    void setPlaybackCursors(const PlaybackCursorFrame& frame);
    void releaseOpenGLResources();
    juce::var automationState() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}

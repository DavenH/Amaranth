#pragma once

#include <JuceHeader.h>

#include "UI/NodePreviewRenderer.h"
#include "Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

struct SignalProbeDetailState {
    String probeId;
    NodePreviewResult renderResult;
    PortDomain domain { PortDomain::TimeSignal };
    RenderScalePolicy scalePolicy { RenderScalePolicy::Unipolar };
    int ordinal {};
    int midiNote { 60 };
    size_t resolution {};
    double releaseDuration {};
    double noteOffTime { 1. };
    uint64_t renderCount {};
    bool releaseDragging {};

    bool isOpen() const { return probeId.isNotEmpty(); }
    double displayedNoteOffTime() const;
    float noteOffPosition() const;
    bool releaseHitTest(Rectangle<float> detailBounds, Point<float> position) const;
    bool beginReleaseDrag(Rectangle<float> detailBounds, Point<float> position);
    void dragRelease(Point<float> position);
    bool endReleaseDrag(Point<float> position);
    bool cancelReleaseDrag();
    void open(
            GraphPreviewResult::SignalProbePreview previewToUse,
            RenderScalePolicy scalePolicyToUse,
            int ordinalToUse,
            int midiNoteToUse,
            size_t resolutionToUse);
    void close() { *this = {}; }

private:
    double dragStartTime { 1. };
    double dragTime { 1. };
    float dragStartX {};
    float dragVoiceWidth { 1.f };
};

class SignalProbeDetailView {
public:
    explicit SignalProbeDetailView(NodePreviewRenderer& rendererToUse) : renderer(rendererToUse) {}

    static Rectangle<float> boundsFor(Rectangle<float> availableContent);
    static Rectangle<float> plotBounds(Rectangle<float> detailBounds);
    static Rectangle<float> voiceBounds(Rectangle<float> detailBounds, const SignalProbeDetailState& state);
    static Rectangle<float> releaseHandleBounds(Rectangle<float> detailBounds, const SignalProbeDetailState& state);
    static Rectangle<float> releaseIconBounds(Rectangle<float> detailBounds, const SignalProbeDetailState& state);
    static bool dismissesOnClick(
            Rectangle<float> detailBounds,
            Point<float> position,
            int clickCount);

    void paint(
            Graphics& graphics,
            Rectangle<float> availableContent,
            const SignalProbeDetailState& state);

private:
    NodePreviewRenderer& renderer;
};

}

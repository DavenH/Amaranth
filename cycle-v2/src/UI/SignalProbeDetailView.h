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

    bool isOpen() const { return probeId.isNotEmpty(); }
    void open(
            GraphPreviewResult::SignalProbePreview previewToUse,
            RenderScalePolicy scalePolicyToUse,
            int ordinalToUse,
            int midiNoteToUse,
            size_t resolutionToUse);
    void close() { *this = {}; }
};

class SignalProbeDetailView {
public:
    explicit SignalProbeDetailView(NodePreviewRenderer& rendererToUse) : renderer(rendererToUse) {}

    static size_t resolutionForMidiNote(int midiNote, double sampleRate = 44100.0);
    static Rectangle<float> boundsFor(Rectangle<float> availableContent);
    static Rectangle<float> closeBounds(Rectangle<float> detailBounds);

    void paint(
            Graphics& graphics,
            Rectangle<float> availableContent,
            const SignalProbeDetailState& state);

private:
    NodePreviewRenderer& renderer;
};

}

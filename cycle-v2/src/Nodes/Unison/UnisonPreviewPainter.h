#pragma once

#include <Audio/CycleDsp/UnisonCore.h>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct UnisonPreviewContext {
    int midiNote { 60 };
    double voiceDurationSeconds { 1.0 };
    std::vector<float> pitchEnvelopeUnitValues;
};

struct UnisonPreviewPath {
    int voiceIndex {};
    float detuneCents {};
    float pan { 0.5f };
    std::vector<CycleDsp::UnisonPhaseSegment> segments;
};

class UnisonPreviewPainter {
public:
    Colour laserColourForPan(float pan) const;
    std::vector<UnisonPreviewPath> makePaths(
            const Node& node,
            const UnisonPreviewContext& context = {}) const;
    void paint(
            Graphics& graphics,
            Rectangle<float> area,
            const Node& node,
            float zoom,
            const UnisonPreviewContext& context = {}) const;
};

}

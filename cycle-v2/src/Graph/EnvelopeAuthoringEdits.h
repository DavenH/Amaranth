#pragma once

#include <array>
#include <vector>

#include "Graph/GraphEditTypes.h"

namespace CycleV2 {

constexpr size_t maximumEnvelopeAuthoringEdits = 64;

enum class EnvelopeAuthoringEditKind {
    Value,
    GuideGain,
    LoopMarker,
    SustainMarker
};

struct EnvelopeAuthoringEdit {
    EnvelopeAuthoringEditKind kind { EnvelopeAuthoringEditKind::Value };
    uint64_t cubeId {};
    int corner {};
    String field;
    float value {};
    bool enabled {};
};

struct EnvelopeAuthoringEdits {
    String nodeId;
    uint64_t expectedDocumentRevision {};
    uint64_t expectedModelRevision {};
    int dimensionCount { -1 };
    std::array<bool, 3> linkedAxes { true, true, true };
    std::vector<EnvelopeAuthoringEdit> edits;
};

}

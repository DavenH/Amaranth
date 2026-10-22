#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <vector>

namespace CycleV2 {

constexpr size_t maximumTrimeshAuthoringEdits = 64;

enum class TrimeshAuthoringEditKind {
    Vertex,
    CubeCurve,
    GuideGain
};

struct TrimeshAuthoringEdit {
    TrimeshAuthoringEditKind kind { TrimeshAuthoringEditKind::Vertex };
    int vertexIndex { -1 };
    int cubeIndex { -1 };
    juce::String field;
    float value {};
};

struct TrimeshAuthoringEdits {
    juce::String nodeId;
    uint64_t expectedDocumentRevision {};
    uint64_t expectedModelRevision {};
    std::vector<TrimeshAuthoringEdit> edits;
};

}

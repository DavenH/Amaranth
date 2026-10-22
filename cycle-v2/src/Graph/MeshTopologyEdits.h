#pragma once

#include "Graph/GraphEditTypes.h"

namespace CycleV2 {

constexpr size_t maximumMeshTopologyEdits = 16;

enum class MeshTopologyEditKind { Insert, Remove };

struct MeshTopologyEdit {
    MeshTopologyEditKind kind { MeshTopologyEditKind::Insert };
    int cubeIndex { -1 };
    uint64_t cubeId {};
    float phase {};
    float amp { 0.5f };
    float curve { 0.5f };
};

struct MeshTopologyEdits {
    String nodeId;
    uint64_t expectedDocumentRevision {};
    uint64_t expectedModelRevision {};
    std::vector<MeshTopologyEdit> edits;
};

}

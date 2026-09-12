#pragma once

#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct GlobalAudioGraphMigrationResult {
    bool migrated {};
    String error;
    std::vector<String> globalNodeIds;

    bool succeeded() const { return error.isEmpty(); }
};

class GlobalAudioGraphMigration {
public:
    static constexpr float laneGap = 96.f;
    static constexpr float nodeClearance = 48.f;
    static constexpr float maximumRowWidth = 1440.f;

    GlobalAudioGraphMigrationResult migrate(NodeGraph& graph) const;
};

}

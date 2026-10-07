#pragma once

#include <memory>
#include <vector>

#include "Graph/GraphEditTypes.h"
#include "Runtime/GraphPresentationSnapshot.h"
#include "Runtime/NodeUpdateGraph.h"

namespace CycleV2 {

class PointPreviewSnapshotPair final {
public:
    void reset(const GraphPresentationSnapshot& published);
    std::shared_ptr<GraphPresentationSnapshot> prepare(
            const GraphPresentationSnapshot& published);
    void recordTouchedSteps(
            const GraphExecutionPlan& plan,
            const GraphChangeSet& change,
            const std::vector<PlannedNodeProduct>& products);
    void publish(GraphPresentationSnapshot& published);
    const GraphPresentationSnapshot& stagingSnapshot() const { return *spare; }

private:
    void synchronize(const GraphPresentationSnapshot& published);

    std::shared_ptr<GraphPresentationSnapshot> spare;
    std::vector<size_t> touchedSteps;
};

}

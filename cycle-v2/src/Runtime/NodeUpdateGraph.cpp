#include "NodeUpdateGraph.h"

namespace CycleV2 {

NodeUpdateResult NodeUpdateGraph::invalidateTrimeshChange(
        const GraphExecutionPlan& plan,
        const String& nodeId,
        const TrimeshChange& change) const {
    NodeUpdateResult result;
    result.trimesh = trimeshInvalidation.invalidate(change);

    result.graph = graphInvalidation.invalidateFrom(
            plan,
            nodeId,
            parameterImpactsFor(result.trimesh));
    return result;
}

ParameterImpact NodeUpdateGraph::parameterImpactsFor(
        const TrimeshInvalidationResult& invalidation) const {
    ParameterImpact impacts = ParameterImpact::None;

    if (invalidation.dirtyCompactPreview) {
        impacts = impacts | ParameterImpact::Preview;
    }
    if (invalidation.dirtyDspPrep) {
        impacts = impacts | ParameterImpact::DspConfiguration;
    }

    return impacts;
}

}

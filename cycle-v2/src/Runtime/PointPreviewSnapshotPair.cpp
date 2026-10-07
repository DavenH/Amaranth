#include <algorithm>

#include "Runtime/PointPreviewSnapshotPair.h"
#include "Graph/InteractionComplexityDiagnostics.h"

namespace CycleV2 {

void PointPreviewSnapshotPair::reset(const GraphPresentationSnapshot& published) {
    InteractionComplexityDiagnostics::recordPresentationSnapshotCopy();
    spare = std::make_shared<GraphPresentationSnapshot>(published);
    touchedSteps.clear();
}

std::shared_ptr<GraphPresentationSnapshot> PointPreviewSnapshotPair::prepare(
        const GraphPresentationSnapshot& published) {
    if (spare == nullptr) {
        reset(published);
    }
    synchronize(published);
    touchedSteps.clear();
    return spare;
}

void PointPreviewSnapshotPair::recordTouchedSteps(
        const GraphExecutionPlan& plan,
        const GraphChangeSet& change,
        const std::vector<PlannedNodeProduct>& products) {
    const auto record = [&](const String& nodeId) {
        const auto found = plan.dependencyIndex.stepIndexById.find(nodeId);
        if (found != plan.dependencyIndex.stepIndexById.end()) {
            touchedSteps.push_back(static_cast<size_t>(found->second));
        }
    };
    for (const auto& nodeId : change.nodeIds) {
        record(nodeId);
    }
    for (const auto& product : products) {
        if (product.product == UpdateProduct::PreviewTraversal
                || product.product == UpdateProduct::CompactPreview
                || product.product == UpdateProduct::ProbePreview
                || product.product == UpdateProduct::AudioConfiguration) {
            record(product.nodeId);
        }
    }
    std::sort(touchedSteps.begin(), touchedSteps.end());
    touchedSteps.erase(std::unique(touchedSteps.begin(), touchedSteps.end()),
            touchedSteps.end());
}

void PointPreviewSnapshotPair::publish(GraphPresentationSnapshot& published) {
    GraphPresentationSnapshot previous = std::move(published);
    published = std::move(*spare);
    *spare = std::move(previous);
    synchronize(published);
    touchedSteps.clear();
}

void PointPreviewSnapshotPair::synchronize(
        const GraphPresentationSnapshot& published) {
    if (spare == nullptr) {
        return;
    }
    spare->graphRevision = published.graphRevision;
    spare->previewMidiNote = published.previewMidiNote;
    spare->previewModWheelValue = published.previewModWheelValue;
    spare->facts = published.facts;
    const auto& plan = published.compileResult.plan;
    const auto& previews = published.previewResult;
    auto& sparePlan = spare->compileResult.plan;
    auto& sparePreviews = spare->previewResult;
    for (const size_t stepIndex : touchedSteps) {
        if (stepIndex >= plan.steps.size() || stepIndex >= sparePlan.steps.size()) {
            continue;
        }
        sparePlan.steps[stepIndex] = plan.steps[stepIndex];
        if (stepIndex < previews.previewResultIndexByStep.size()
                && stepIndex < sparePreviews.previewResultIndexByStep.size()) {
            const int sourceIndex = previews.previewResultIndexByStep[stepIndex];
            const int destinationIndex = sparePreviews.previewResultIndexByStep[stepIndex];
            if (sourceIndex >= 0 && destinationIndex >= 0
                    && static_cast<size_t>(sourceIndex) < previews.nodes.size()
                    && static_cast<size_t>(destinationIndex) < sparePreviews.nodes.size()) {
                sparePreviews.nodes[static_cast<size_t>(destinationIndex)]
                        = previews.nodes[static_cast<size_t>(sourceIndex)];
            }
        }
        if (plan.observationIndex != nullptr
                && stepIndex < plan.observationIndex->probeIndicesByStep.size()) {
            for (const size_t probeIndex :
                    plan.observationIndex->probeIndicesByStep[stepIndex]) {
                if (probeIndex < previews.probes.size()
                        && probeIndex < sparePreviews.probes.size()) {
                    sparePreviews.probes[probeIndex] = previews.probes[probeIndex];
                }
            }
        }
        if (plan.defaultOutputProbe.has_value()
                && plan.defaultOutputProbe->sourceStepIndex
                        == static_cast<int>(stepIndex)) {
            sparePreviews.defaultOutput = previews.defaultOutput;
            sparePreviews.defaultOutputSpectrum = previews.defaultOutputSpectrum;
        }
    }
    sparePreviews.indexedNodeCount = previews.indexedNodeCount;
    sparePreviews.addressLookupCount = previews.addressLookupCount;
    sparePreviews.aliasedInputCount = previews.aliasedInputCount;
    sparePreviews.reusedCapturedTraversalCount = previews.reusedCapturedTraversalCount;
    sparePreviews.renderedNodeCount = previews.renderedNodeCount;
}

}

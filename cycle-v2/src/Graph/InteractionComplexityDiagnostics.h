#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace CycleV2 {

struct InteractionComplexityCounts {
    uint64_t graphCopies {};
    uint64_t audioSamplesCopied {};
    uint64_t meshCopies {};
    uint64_t meshVerticesCopied {};
    uint64_t meshCubesCopied {};
    uint64_t modelSerializations {};
    uint64_t editorStateComparisons {};
    uint64_t nodeLinearScans {};
    uint64_t parameterLinearScans {};
    uint64_t assignmentLinearScans {};
    uint64_t meshEditOwnerVisits {};
    uint64_t validationNodeVisits {};
    uint64_t validationEdgeVisits {};
    uint64_t domainTransfers {};
    uint64_t presentationSnapshotCopies {};
    uint64_t overlayNodesMaterialized {};
    uint64_t previewPreparationStepVisits {};
    uint64_t previewPlanningSlotVisits {};
    uint64_t previewRenderStepVisits {};
    uint64_t presentationIndexRebuildVisits {};
};

class InteractionComplexityDiagnostics {
public:
    static void reset() {
        graphCopies.store(0, std::memory_order_relaxed);
        audioSamplesCopied.store(0, std::memory_order_relaxed);
        meshCopies.store(0, std::memory_order_relaxed);
        meshVerticesCopied.store(0, std::memory_order_relaxed);
        meshCubesCopied.store(0, std::memory_order_relaxed);
        modelSerializations.store(0, std::memory_order_relaxed);
        editorStateComparisons.store(0, std::memory_order_relaxed);
        nodeLinearScans.store(0, std::memory_order_relaxed);
        parameterLinearScans.store(0, std::memory_order_relaxed);
        assignmentLinearScans.store(0, std::memory_order_relaxed);
        meshEditOwnerVisits.store(0, std::memory_order_relaxed);
        validationNodeVisits.store(0, std::memory_order_relaxed);
        validationEdgeVisits.store(0, std::memory_order_relaxed);
        domainTransfers.store(0, std::memory_order_relaxed);
        presentationSnapshotCopies.store(0, std::memory_order_relaxed);
        overlayNodesMaterialized.store(0, std::memory_order_relaxed);
        previewPreparationStepVisits.store(0, std::memory_order_relaxed);
        previewPlanningSlotVisits.store(0, std::memory_order_relaxed);
        previewRenderStepVisits.store(0, std::memory_order_relaxed);
        presentationIndexRebuildVisits.store(0, std::memory_order_relaxed);
    }

    static InteractionComplexityCounts counts() {
        return {
                graphCopies.load(std::memory_order_relaxed),
                audioSamplesCopied.load(std::memory_order_relaxed),
                meshCopies.load(std::memory_order_relaxed),
                meshVerticesCopied.load(std::memory_order_relaxed),
                meshCubesCopied.load(std::memory_order_relaxed),
                modelSerializations.load(std::memory_order_relaxed),
                editorStateComparisons.load(std::memory_order_relaxed),
                nodeLinearScans.load(std::memory_order_relaxed),
                parameterLinearScans.load(std::memory_order_relaxed),
                assignmentLinearScans.load(std::memory_order_relaxed),
                meshEditOwnerVisits.load(std::memory_order_relaxed),
                validationNodeVisits.load(std::memory_order_relaxed),
                validationEdgeVisits.load(std::memory_order_relaxed),
                domainTransfers.load(std::memory_order_relaxed),
                presentationSnapshotCopies.load(std::memory_order_relaxed),
                overlayNodesMaterialized.load(std::memory_order_relaxed),
                previewPreparationStepVisits.load(std::memory_order_relaxed),
                previewPlanningSlotVisits.load(std::memory_order_relaxed),
                previewRenderStepVisits.load(std::memory_order_relaxed),
                presentationIndexRebuildVisits.load(std::memory_order_relaxed)
        };
    }

    static void recordGraphCopy() { ++graphCopies; }
    static void recordAudioSampleCopy(size_t count) { audioSamplesCopied += count; }
    static void recordMeshCopy(size_t vertices, size_t cubes) {
        ++meshCopies;
        meshVerticesCopied += vertices;
        meshCubesCopied += cubes;
    }
    static void recordModelSerialization() { ++modelSerializations; }
    static void recordEditorStateComparison() { ++editorStateComparisons; }
    static void recordNodeLinearScan() { ++nodeLinearScans; }
    static void recordParameterLinearScan() { ++parameterLinearScans; }
    static void recordAssignmentLinearScan() { ++assignmentLinearScans; }
    static void recordMeshEditOwnerVisit() { ++meshEditOwnerVisits; }
    static void recordValidationNodeVisits(size_t count) { validationNodeVisits += count; }
    static void recordValidationEdgeVisits(size_t count) { validationEdgeVisits += count; }
    static void recordDomainTransfer() { ++domainTransfers; }
    static void recordPresentationSnapshotCopy() { ++presentationSnapshotCopies; }
    static void recordOverlayNodeMaterialization(size_t count) {
        overlayNodesMaterialized += count;
    }
    static void recordPreviewPreparationStepVisit() { ++previewPreparationStepVisits; }
    static void recordPreviewPlanningSlotVisits(size_t count) {
        previewPlanningSlotVisits += count;
    }
    static void recordPreviewRenderStepVisit() { ++previewRenderStepVisits; }
    static void recordPresentationIndexRebuildVisits(size_t count) {
        presentationIndexRebuildVisits += count;
    }

private:
    static inline std::atomic<uint64_t> graphCopies {};
    static inline std::atomic<uint64_t> audioSamplesCopied {};
    static inline std::atomic<uint64_t> meshCopies {};
    static inline std::atomic<uint64_t> meshVerticesCopied {};
    static inline std::atomic<uint64_t> meshCubesCopied {};
    static inline std::atomic<uint64_t> modelSerializations {};
    static inline std::atomic<uint64_t> editorStateComparisons {};
    static inline std::atomic<uint64_t> nodeLinearScans {};
    static inline std::atomic<uint64_t> parameterLinearScans {};
    static inline std::atomic<uint64_t> assignmentLinearScans {};
    static inline std::atomic<uint64_t> meshEditOwnerVisits {};
    static inline std::atomic<uint64_t> validationNodeVisits {};
    static inline std::atomic<uint64_t> validationEdgeVisits {};
    static inline std::atomic<uint64_t> domainTransfers {};
    static inline std::atomic<uint64_t> presentationSnapshotCopies {};
    static inline std::atomic<uint64_t> overlayNodesMaterialized {};
    static inline std::atomic<uint64_t> previewPreparationStepVisits {};
    static inline std::atomic<uint64_t> previewPlanningSlotVisits {};
    static inline std::atomic<uint64_t> previewRenderStepVisits {};
    static inline std::atomic<uint64_t> presentationIndexRebuildVisits {};
};

}

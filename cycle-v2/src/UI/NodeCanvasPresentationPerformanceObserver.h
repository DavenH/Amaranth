#pragma once

#include <cstdint>

namespace CycleV2 {

enum class NodeCanvasPresentationStage : uint8_t {
    FramePreparation,
    Backdrop,
    SnapGuides,
    Cables,
    CableBodies,
    CableAnnotations,
    PendingConnection,
    Nodes,
    RelationshipHighlights,
    Utilities,
    MiniMap,
    Legend,
    Palette,
    Status,
    GuideShelf,
    SpyRail,
    SpyRailPreviews,
    DockAndDetail,
    Count
};

class NodeCanvasPresentationPerformanceObserver {
public:
    virtual ~NodeCanvasPresentationPerformanceObserver() = default;

    virtual uint64_t presentationTimestamp() const = 0;
    virtual void presentationStageCompleted(
            NodeCanvasPresentationStage stage,
            uint64_t elapsedMicroseconds) = 0;
    virtual void nodeLayerCacheCompleted(
            uint64_t hits,
            uint64_t misses,
            uint64_t elapsedMicroseconds) = 0;
    virtual void cableLayerCacheCompleted(
            uint64_t hits,
            uint64_t misses,
            uint64_t elapsedMicroseconds) = 0;
    virtual void cableCompositeCacheCompleted(
            bool hit,
            uint64_t elapsedMicroseconds) = 0;
    virtual void spyPreviewTileCacheCompleted(
            uint64_t hits,
            uint64_t misses,
            uint64_t elapsedMicroseconds) = 0;
};

class ScopedNodeCanvasPresentationStage final {
public:
    ScopedNodeCanvasPresentationStage(
            NodeCanvasPresentationPerformanceObserver* observerToUse,
            NodeCanvasPresentationStage stageToMeasure) :
            observer(observerToUse)
        ,   stage(stageToMeasure)
        ,   startedAt(observer != nullptr ? observer->presentationTimestamp() : 0) {
    }

    ~ScopedNodeCanvasPresentationStage() {
        if (observer != nullptr) {
            observer->presentationStageCompleted(
                    stage,
                    observer->presentationTimestamp() - startedAt);
        }
    }

    ScopedNodeCanvasPresentationStage(const ScopedNodeCanvasPresentationStage&) = delete;
    ScopedNodeCanvasPresentationStage& operator=(
            const ScopedNodeCanvasPresentationStage&) = delete;

private:
    NodeCanvasPresentationPerformanceObserver* observer;
    NodeCanvasPresentationStage stage;
    uint64_t startedAt;
};

}

#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>

#include "Runtime/PerformanceDistribution.h"
#include "UI/NodeCanvasPresentationPerformanceObserver.h"
#include "UI/NodeEditorPerformanceObserver.h"
#include "UI/RenderInvalidationAccumulator.h"

namespace CycleV2 {

class CanvasPerformanceMetrics final :
        public NodeEditorPerformanceObserver
    ,   public NodeCanvasPresentationPerformanceObserver {
public:
    enum class Trigger : uint8_t {
        Hover,
        PointerGesture,
        Viewport,
        GraphEdit,
        ParameterEdit,
        PreviewRuntime,
        Timer,
        LayoutLifecycle,
        Other,
        Count
    };

    enum class Frame : uint8_t {
        JucePaint,
        OpenGlRender,
        Count
    };

    enum class RepaintScope : uint8_t {
        Canvas,
        Status,
        Count
    };

    enum class Operation : uint8_t {
        HoverResolution,
        NodeParameterUpdate,
        NodeParameterCommit,
        TrimeshPointUpdate,
        TrimeshPointCommit,
        TrimeshVertexUpdate,
        TrimeshVertexCommit,
        TrimeshVertexSelection,
        Count
    };

    static constexpr size_t triggerCount = static_cast<size_t>(Trigger::Count);
    static constexpr size_t frameCount = static_cast<size_t>(Frame::Count);
    static constexpr size_t repaintScopeCount = static_cast<size_t>(RepaintScope::Count);
    static constexpr size_t operationCount = static_cast<size_t>(Operation::Count);
    static constexpr size_t presentationStageCount = static_cast<size_t>(
            NodeCanvasPresentationStage::Count);
    using Distribution = PerformanceDistribution;

    struct TriggerSnapshot {
        uint64_t invocations {};
        uint64_t repaintRequests {};
        uint64_t repaintPaints {};
        Distribution handlerDuration;
        Distribution repaintLatency;
    };

    struct Snapshot {
        uint64_t elapsedMicroseconds {};
        std::array<TriggerSnapshot, triggerCount> triggers;
        std::array<Distribution, frameCount> frames;
        std::array<uint64_t, repaintScopeCount> repaintScopes {};
        std::array<Distribution, operationCount> operations;
        std::array<Distribution, presentationStageCount> presentationStages;
        uint64_t nodeLayerCacheHits {};
        uint64_t nodeLayerCacheMisses {};
        Distribution nodeLayerCacheHitDuration;
        Distribution nodeLayerCacheMissDuration;
        uint64_t cableLayerCacheHits {};
        uint64_t cableLayerCacheMisses {};
        Distribution cableLayerCacheHitDuration;
        Distribution cableLayerCacheMissDuration;
        uint64_t cableCompositeCacheHits {};
        uint64_t cableCompositeCacheMisses {};
        Distribution cableCompositeCacheHitDuration;
        Distribution cableCompositeCacheMissDuration;
        uint64_t spyPreviewTileCacheHits {};
        uint64_t spyPreviewTileCacheMisses {};
        Distribution spyPreviewTileCacheHitDuration;
        Distribution spyPreviewTileCacheMissDuration;
        uint64_t hoverStateChanges {};
        uint64_t hoverStateUnchanged {};
        uint64_t occludedHoverResolutions {};
    };

    using Clock = uint64_t (*)();

    class ScopedTrigger final {
    public:
        ScopedTrigger(CanvasPerformanceMetrics& owner, Trigger trigger);
        ~ScopedTrigger();

        ScopedTrigger(ScopedTrigger&& other) noexcept;
        ScopedTrigger& operator=(ScopedTrigger&&) = delete;

    private:
        CanvasPerformanceMetrics* metrics;
        CanvasPerformanceMetrics* previousMetrics;
        Trigger measuredTrigger;
        Trigger previousTrigger;
        uint64_t startMicroseconds;

        JUCE_DECLARE_NON_COPYABLE(ScopedTrigger)
    };

    class ScopedFrame final {
    public:
        ScopedFrame(CanvasPerformanceMetrics& owner, Frame frame);
        ~ScopedFrame();

        ScopedFrame(ScopedFrame&& other) noexcept;
        ScopedFrame& operator=(ScopedFrame&&) = delete;

    private:
        CanvasPerformanceMetrics* metrics;
        Frame measuredFrame;
        uint64_t startMicroseconds;

        JUCE_DECLARE_NON_COPYABLE(ScopedFrame)
    };

    explicit CanvasPerformanceMetrics(Clock clock = defaultClock);

    ScopedTrigger measure(Trigger trigger);
    ScopedFrame measure(Frame frame);
    uint64_t timestamp() const { return now(); }
    void requestRepaint(RepaintScope scope = RepaintScope::Canvas);
    void requestRepaint(Trigger trigger, RepaintScope scope = RepaintScope::Canvas);
    void recordOperation(Operation operation, uint64_t elapsedMicroseconds);
    void recordHoverState(bool changed, bool occluded = false);
    void nodeEditorOperationCompleted(
            NodeEditorPerformanceOperation operation,
            uint64_t elapsedMicroseconds) override;
    uint64_t presentationTimestamp() const override { return timestamp(); }
    void presentationStageCompleted(
            NodeCanvasPresentationStage stage,
            uint64_t elapsedMicroseconds) override;
    void nodeLayerCacheCompleted(
            uint64_t hits,
            uint64_t misses,
            uint64_t elapsedMicroseconds) override;
    void cableLayerCacheCompleted(
            uint64_t hits,
            uint64_t misses,
            uint64_t elapsedMicroseconds) override;
    void cableCompositeCacheCompleted(
            bool hit,
            uint64_t elapsedMicroseconds) override;
    void spyPreviewTileCacheCompleted(
            uint64_t hits,
            uint64_t misses,
            uint64_t elapsedMicroseconds) override;
    void reset();

    Snapshot snapshot() const;
    juce::var toVar(const RenderInvalidationAccumulator::Diagnostics& invalidation) const;

    static const char* label(Trigger trigger);
    static const char* label(Frame frame);
    static const char* label(RepaintScope scope);
    static const char* label(Operation operation);
    static const char* label(NodeCanvasPresentationStage stage);
    static double percentileMilliseconds(const Distribution& distribution, double percentile);

private:
    struct TriggerData {
        uint64_t invocations {};
        uint64_t repaintRequests {};
        uint64_t repaintPaints {};
        uint64_t firstPendingRepaintMicroseconds {};
        Distribution handlerDuration;
        Distribution repaintLatency;
    };

    static uint64_t defaultClock();
    static void record(Distribution& distribution, uint64_t microseconds);
    static juce::var distributionToVar(const Distribution& distribution);

    void finishTrigger(Trigger trigger, uint64_t startMicroseconds);
    void beginFrame(Frame frame, uint64_t nowMicroseconds);
    void finishFrame(Frame frame, uint64_t startMicroseconds);

    Clock now;
    mutable juce::CriticalSection lock;
    uint64_t windowStartMicroseconds {};
    std::array<TriggerData, triggerCount> triggerData;
    std::array<Distribution, frameCount> frameData;
    std::array<uint64_t, repaintScopeCount> repaintScopeData {};
    std::array<Distribution, operationCount> operationData;
    std::array<Distribution, presentationStageCount> presentationStageData;
    uint64_t nodeLayerCacheHits {};
    uint64_t nodeLayerCacheMisses {};
    Distribution nodeLayerCacheHitDuration;
    Distribution nodeLayerCacheMissDuration;
    uint64_t cableLayerCacheHits {};
    uint64_t cableLayerCacheMisses {};
    Distribution cableLayerCacheHitDuration;
    Distribution cableLayerCacheMissDuration;
    uint64_t cableCompositeCacheHits {};
    uint64_t cableCompositeCacheMisses {};
    Distribution cableCompositeCacheHitDuration;
    Distribution cableCompositeCacheMissDuration;
    uint64_t spyPreviewTileCacheHits {};
    uint64_t spyPreviewTileCacheMisses {};
    Distribution spyPreviewTileCacheHitDuration;
    Distribution spyPreviewTileCacheMissDuration;
    uint64_t hoverStateChanges {};
    uint64_t hoverStateUnchanged {};
    uint64_t occludedHoverResolutions {};

    static thread_local CanvasPerformanceMetrics* activeMetrics;
    static thread_local Trigger activeTrigger;
};

}

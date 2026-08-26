#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>

#include "UI/RenderInvalidationAccumulator.h"

namespace CycleV2 {

class CanvasPerformanceMetrics final {
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

    static constexpr size_t triggerCount = static_cast<size_t>(Trigger::Count);
    static constexpr size_t frameCount = static_cast<size_t>(Frame::Count);
    static constexpr size_t histogramBucketCount = 13;

    struct Distribution {
        uint64_t count {};
        uint64_t totalMicroseconds {};
        uint64_t maximumMicroseconds {};
        std::array<uint64_t, histogramBucketCount> buckets {};
    };

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
    void requestRepaint();
    void requestRepaint(Trigger trigger);
    void reset();

    Snapshot snapshot() const;
    juce::var toVar(const RenderInvalidationAccumulator::Diagnostics& invalidation) const;

    static const char* label(Trigger trigger);
    static const char* label(Frame frame);
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
    static size_t bucketFor(uint64_t microseconds);
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

    static thread_local CanvasPerformanceMetrics* activeMetrics;
    static thread_local Trigger activeTrigger;
};

}

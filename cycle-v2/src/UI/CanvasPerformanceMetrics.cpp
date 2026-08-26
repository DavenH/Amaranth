#include "UI/CanvasPerformanceMetrics.h"

#include <algorithm>
#include <cmath>

namespace CycleV2 {

using namespace juce;

namespace {

constexpr std::array<uint64_t, CanvasPerformanceMetrics::histogramBucketCount - 1>
        bucketUpperMicroseconds {
            50, 100, 250, 500, 1000, 2000, 4000, 8000, 16000, 33000, 66000, 100000
        };

size_t indexFor(CanvasPerformanceMetrics::Trigger trigger) {
    return static_cast<size_t>(trigger);
}

size_t indexFor(CanvasPerformanceMetrics::Frame frame) {
    return static_cast<size_t>(frame);
}

}

thread_local CanvasPerformanceMetrics* CanvasPerformanceMetrics::activeMetrics = nullptr;
thread_local CanvasPerformanceMetrics::Trigger CanvasPerformanceMetrics::activeTrigger =
        CanvasPerformanceMetrics::Trigger::Other;

CanvasPerformanceMetrics::ScopedTrigger::ScopedTrigger(
        CanvasPerformanceMetrics& owner,
        Trigger trigger) :
        metrics(&owner)
    ,   previousMetrics(activeMetrics)
    ,   measuredTrigger(trigger)
    ,   previousTrigger(activeTrigger)
    ,   startMicroseconds(owner.now()) {
    activeMetrics = metrics;
    activeTrigger = measuredTrigger;
}

CanvasPerformanceMetrics::ScopedTrigger::~ScopedTrigger() {
    if (metrics == nullptr) {
        return;
    }

    metrics->finishTrigger(measuredTrigger, startMicroseconds);
    activeMetrics = previousMetrics;
    activeTrigger = previousTrigger;
}

CanvasPerformanceMetrics::ScopedTrigger::ScopedTrigger(ScopedTrigger&& other) noexcept :
        metrics(other.metrics)
    ,   previousMetrics(other.previousMetrics)
    ,   measuredTrigger(other.measuredTrigger)
    ,   previousTrigger(other.previousTrigger)
    ,   startMicroseconds(other.startMicroseconds) {
    other.metrics = nullptr;
}

CanvasPerformanceMetrics::ScopedFrame::ScopedFrame(
        CanvasPerformanceMetrics& owner,
        Frame frame) :
        metrics(&owner)
    ,   measuredFrame(frame)
    ,   startMicroseconds(owner.now()) {
    metrics->beginFrame(measuredFrame, startMicroseconds);
}

CanvasPerformanceMetrics::ScopedFrame::~ScopedFrame() {
    if (metrics != nullptr) {
        metrics->finishFrame(measuredFrame, startMicroseconds);
    }
}

CanvasPerformanceMetrics::ScopedFrame::ScopedFrame(ScopedFrame&& other) noexcept :
        metrics(other.metrics)
    ,   measuredFrame(other.measuredFrame)
    ,   startMicroseconds(other.startMicroseconds) {
    other.metrics = nullptr;
}

CanvasPerformanceMetrics::CanvasPerformanceMetrics(Clock clock) :
        now(clock)
    ,   windowStartMicroseconds(now()) {
}

CanvasPerformanceMetrics::ScopedTrigger CanvasPerformanceMetrics::measure(Trigger trigger) {
    return ScopedTrigger(*this, trigger);
}

CanvasPerformanceMetrics::ScopedFrame CanvasPerformanceMetrics::measure(Frame frame) {
    return ScopedFrame(*this, frame);
}

void CanvasPerformanceMetrics::requestRepaint() {
    requestRepaint(activeMetrics == this ? activeTrigger : Trigger::Other);
}

void CanvasPerformanceMetrics::requestRepaint(Trigger trigger) {
    const uint64_t timestamp = now();
    const juce::ScopedLock scopedLock(lock);
    auto& data = triggerData[indexFor(trigger)];
    ++data.repaintRequests;
    if (data.firstPendingRepaintMicroseconds == 0) {
        data.firstPendingRepaintMicroseconds = timestamp;
    }
}

void CanvasPerformanceMetrics::reset() {
    const uint64_t timestamp = now();
    const juce::ScopedLock scopedLock(lock);
    windowStartMicroseconds = timestamp;
    triggerData = {};
    frameData = {};
}

CanvasPerformanceMetrics::Snapshot CanvasPerformanceMetrics::snapshot() const {
    Snapshot result;
    const uint64_t timestamp = now();
    const juce::ScopedLock scopedLock(lock);
    result.elapsedMicroseconds = timestamp - windowStartMicroseconds;
    for (size_t index = 0; index < triggerCount; ++index) {
        result.triggers[index] = {
                triggerData[index].invocations,
                triggerData[index].repaintRequests,
                triggerData[index].repaintPaints,
                triggerData[index].handlerDuration,
                triggerData[index].repaintLatency
        };
    }
    result.frames = frameData;
    return result;
}

var CanvasPerformanceMetrics::toVar(
        const RenderInvalidationAccumulator::Diagnostics& invalidation) const {
    const Snapshot current = snapshot();
    auto* root = new DynamicObject();
    root->setProperty("schema", "cycle-v2-canvas-performance.v1");
    root->setProperty("elapsedMs", (double) current.elapsedMicroseconds / 1000.0);

    uint64_t totalInvocations {};
    uint64_t totalRequests {};
    for (const auto& trigger : current.triggers) {
        totalInvocations += trigger.invocations;
        totalRequests += trigger.repaintRequests;
    }
    root->setProperty("triggerInvocations", (int64) totalInvocations);
    root->setProperty("repaintRequests", (int64) totalRequests);
    root->setProperty("repaintPaints", (int64) current.frames[indexFor(Frame::JucePaint)].count);
    root->setProperty(
            "requestsPerInvalidationFlush",
            invalidation.completedFlushes == 0
                    ? 0.0
                    : (double) totalRequests / (double) invalidation.completedFlushes);

    Array<var> triggers;
    for (size_t index = 0; index < triggerCount; ++index) {
        const auto trigger = static_cast<Trigger>(index);
        const auto& data = current.triggers[index];
        auto* object = new DynamicObject();
        object->setProperty("name", label(trigger));
        object->setProperty("invocations", (int64) data.invocations);
        object->setProperty(
                "invocationShare",
                totalInvocations == 0 ? 0.0 : (double) data.invocations / (double) totalInvocations);
        object->setProperty("repaintRequests", (int64) data.repaintRequests);
        object->setProperty(
                "repaintRequestShare",
                totalRequests == 0 ? 0.0 : (double) data.repaintRequests / (double) totalRequests);
        object->setProperty("repaintPaints", (int64) data.repaintPaints);
        object->setProperty("handlerDuration", distributionToVar(data.handlerDuration));
        object->setProperty("repaintLatency", distributionToVar(data.repaintLatency));
        triggers.add(var(object));
    }
    root->setProperty("triggers", triggers);

    auto* frames = new DynamicObject();
    for (size_t index = 0; index < frameCount; ++index) {
        const auto frame = static_cast<Frame>(index);
        frames->setProperty(label(frame), distributionToVar(current.frames[index]));
    }
    root->setProperty("frames", var(frames));

    auto* invalidationObject = new DynamicObject();
    invalidationObject->setProperty("requests", (int64) invalidation.requests);
    invalidationObject->setProperty("scheduledFlushes", (int64) invalidation.scheduledFlushes);
    invalidationObject->setProperty("completedFlushes", (int64) invalidation.completedFlushes);
    invalidationObject->setProperty("categoryDispatches", (int64) invalidation.categoryDispatches);
    root->setProperty("invalidation", var(invalidationObject));
    return var(root);
}

const char* CanvasPerformanceMetrics::label(Trigger trigger) {
    switch (trigger) {
        case Trigger::Hover:             return "hover";
        case Trigger::PointerGesture:    return "pointerGesture";
        case Trigger::Viewport:          return "viewport";
        case Trigger::GraphEdit:         return "graphEdit";
        case Trigger::ParameterEdit:     return "parameterEdit";
        case Trigger::PreviewRuntime:    return "previewRuntime";
        case Trigger::Timer:             return "timer";
        case Trigger::LayoutLifecycle:   return "layoutLifecycle";
        case Trigger::Other:             return "other";
        case Trigger::Count:             break;
    }
    return "unknown";
}

const char* CanvasPerformanceMetrics::label(Frame frame) {
    switch (frame) {
        case Frame::JucePaint:       return "jucePaint";
        case Frame::OpenGlRender:    return "openGlRender";
        case Frame::Count:           break;
    }
    return "unknown";
}

double CanvasPerformanceMetrics::percentileMilliseconds(
        const Distribution& distribution,
        double percentile) {
    if (distribution.count == 0) {
        return 0.0;
    }

    const uint64_t target = std::max<uint64_t>(
            1,
            (uint64_t) std::ceil((double) distribution.count * percentile));
    uint64_t cumulative {};
    for (size_t index = 0; index < distribution.buckets.size(); ++index) {
        cumulative += distribution.buckets[index];
        if (cumulative >= target) {
            const uint64_t upper = index < bucketUpperMicroseconds.size()
                    ? bucketUpperMicroseconds[index]
                    : distribution.maximumMicroseconds;
            return (double) upper / 1000.0;
        }
    }
    return (double) distribution.maximumMicroseconds / 1000.0;
}

uint64_t CanvasPerformanceMetrics::defaultClock() {
    return (uint64_t) (Time::getMillisecondCounterHiRes() * 1000.0);
}

size_t CanvasPerformanceMetrics::bucketFor(uint64_t microseconds) {
    const auto found = std::lower_bound(
            bucketUpperMicroseconds.begin(),
            bucketUpperMicroseconds.end(),
            microseconds);
    return (size_t) std::distance(bucketUpperMicroseconds.begin(), found);
}

void CanvasPerformanceMetrics::record(
        Distribution& distribution,
        uint64_t microseconds) {
    ++distribution.count;
    distribution.totalMicroseconds += microseconds;
    distribution.maximumMicroseconds = std::max(
            distribution.maximumMicroseconds,
            microseconds);
    ++distribution.buckets[bucketFor(microseconds)];
}

var CanvasPerformanceMetrics::distributionToVar(const Distribution& distribution) {
    auto* object = new DynamicObject();
    object->setProperty("count", (int64) distribution.count);
    object->setProperty(
            "meanMs",
            distribution.count == 0
                    ? 0.0
                    : (double) distribution.totalMicroseconds / (double) distribution.count / 1000.0);
    object->setProperty("p50Ms", percentileMilliseconds(distribution, 0.50));
    object->setProperty("p95Ms", percentileMilliseconds(distribution, 0.95));
    object->setProperty("p99Ms", percentileMilliseconds(distribution, 0.99));
    object->setProperty("maxMs", (double) distribution.maximumMicroseconds / 1000.0);
    return var(object);
}

void CanvasPerformanceMetrics::finishTrigger(
        Trigger trigger,
        uint64_t startMicroseconds) {
    const uint64_t elapsed = now() - startMicroseconds;
    const juce::ScopedLock scopedLock(lock);
    auto& data = triggerData[indexFor(trigger)];
    ++data.invocations;
    record(data.handlerDuration, elapsed);
}

void CanvasPerformanceMetrics::beginFrame(Frame frame, uint64_t nowMicroseconds) {
    if (frame != Frame::JucePaint) {
        return;
    }

    const juce::ScopedLock scopedLock(lock);
    for (auto& data : triggerData) {
        if (data.firstPendingRepaintMicroseconds == 0) {
            continue;
        }
        ++data.repaintPaints;
        record(data.repaintLatency, nowMicroseconds - data.firstPendingRepaintMicroseconds);
        data.firstPendingRepaintMicroseconds = 0;
    }
}

void CanvasPerformanceMetrics::finishFrame(Frame frame, uint64_t startMicroseconds) {
    const uint64_t elapsed = now() - startMicroseconds;
    const juce::ScopedLock scopedLock(lock);
    record(frameData[indexFor(frame)], elapsed);
}

}

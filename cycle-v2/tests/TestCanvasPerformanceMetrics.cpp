#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "UI/CanvasPerformanceMetrics.h"
#include "Runtime/GraphPresentationPerformanceMetrics.h"

using namespace CycleV2;
using namespace juce;

namespace {

uint64_t fakeNow {};

uint64_t fakeClock() {
    return fakeNow;
}

const var& property(const var& value, const Identifier& name) {
    return value.getDynamicObject()->getProperties()[name];
}

const var& triggerWithName(const var& metrics, const String& name) {
    const Array<var>* triggers = property(metrics, "triggers").getArray();
    REQUIRE(triggers != nullptr);
    for (const auto& trigger : *triggers) {
        if (property(trigger, "name").toString() == name) {
            return trigger;
        }
    }
    FAIL("Missing performance trigger " << name);
}

}

TEST_CASE("Canvas metrics attribute coalesced repaint latency to its editing trigger",
        "[cycle-v2][canvas][performance]") {
    fakeNow = 100;
    CanvasPerformanceMetrics metrics(fakeClock);

    fakeNow = 200;
    {
        auto trigger = metrics.measure(CanvasPerformanceMetrics::Trigger::Hover);
        fakeNow = 300;
        metrics.requestRepaint();
        fakeNow = 400;
        metrics.requestRepaint();
        fakeNow = 700;
    }
    fakeNow = 1200;
    {
        auto paint = metrics.measure(CanvasPerformanceMetrics::Frame::JucePaint);
        fakeNow = 1700;
    }

    const auto snapshot = metrics.snapshot();
    const auto& hover = snapshot.triggers[
            static_cast<size_t>(CanvasPerformanceMetrics::Trigger::Hover)];
    const auto& paint = snapshot.frames[
            static_cast<size_t>(CanvasPerformanceMetrics::Frame::JucePaint)];

    REQUIRE(hover.invocations == 1);
    REQUIRE(hover.repaintRequests == 2);
    REQUIRE(hover.repaintPaints == 1);
    REQUIRE(hover.handlerDuration.totalMicroseconds == 500);
    REQUIRE(hover.repaintLatency.totalMicroseconds == 900);
    REQUIRE(paint.count == 1);
    REQUIRE(paint.totalMicroseconds == 500);
}

TEST_CASE("Canvas metrics export trigger distributions and invalidation coalescing",
        "[cycle-v2][canvas][performance]") {
    fakeNow = 1000;
    CanvasPerformanceMetrics metrics(fakeClock);

    {
        auto hover = metrics.measure(CanvasPerformanceMetrics::Trigger::Hover);
        fakeNow = 1100;
    }
    {
        auto edit = metrics.measure(CanvasPerformanceMetrics::Trigger::ParameterEdit);
        metrics.requestRepaint();
        fakeNow = 2100;
    }

    RenderInvalidationAccumulator::Diagnostics invalidation;
    invalidation.requests = 3;
    invalidation.scheduledFlushes = 1;
    invalidation.completedFlushes = 1;
    invalidation.categoryDispatches = 1;
    const var exported = metrics.toVar(invalidation);
    const var& hover = triggerWithName(exported, "hover");
    const var& parameter = triggerWithName(exported, "parameterEdit");

    REQUIRE((int64) property(exported, "triggerInvocations") == 2);
    REQUIRE((int64) property(exported, "repaintRequests") == 1);
    REQUIRE((double) property(exported, "requestsPerInvalidationFlush")
            == Catch::Approx(1.0));
    REQUIRE((double) property(hover, "invocationShare") == Catch::Approx(0.5));
    REQUIRE((double) property(parameter, "invocationShare") == Catch::Approx(0.5));
    REQUIRE((int64) property(parameter, "repaintRequests") == 1);

    const var& duration = property(parameter, "handlerDuration");
    REQUIRE((double) property(duration, "meanMs") == Catch::Approx(1.0));
    REQUIRE((double) property(duration, "p95Ms") == Catch::Approx(1.0));
}

TEST_CASE("Canvas metrics reset starts an empty independent observation window",
        "[cycle-v2][canvas][performance]") {
    fakeNow = 100;
    CanvasPerformanceMetrics metrics(fakeClock);
    metrics.requestRepaint(CanvasPerformanceMetrics::Trigger::GraphEdit);

    fakeNow = 500;
    metrics.reset();
    fakeNow = 900;

    const auto snapshot = metrics.snapshot();
    REQUIRE(snapshot.elapsedMicroseconds == 400);
    for (const auto& trigger : snapshot.triggers) {
        REQUIRE(trigger.invocations == 0);
        REQUIRE(trigger.repaintRequests == 0);
        REQUIRE(trigger.repaintPaints == 0);
        REQUIRE(trigger.handlerDuration.count == 0);
        REQUIRE(trigger.repaintLatency.count == 0);
    }
    for (const uint64_t scopeRequests : snapshot.repaintScopes) {
        REQUIRE(scopeRequests == 0);
    }
}

TEST_CASE("Canvas metrics distinguish full and status-region repaint requests",
        "[cycle-v2][canvas][performance]") {
    fakeNow = 100;
    CanvasPerformanceMetrics metrics(fakeClock);

    metrics.requestRepaint(CanvasPerformanceMetrics::RepaintScope::Status);
    metrics.requestRepaint(CanvasPerformanceMetrics::Trigger::GraphEdit);

    const auto snapshot = metrics.snapshot();
    REQUIRE(snapshot.repaintScopes[static_cast<size_t>(
            CanvasPerformanceMetrics::RepaintScope::Canvas)] == 1);
    REQUIRE(snapshot.repaintScopes[static_cast<size_t>(
            CanvasPerformanceMetrics::RepaintScope::Status)] == 1);

    const var exported = metrics.toVar({});
    const var& repaintScopes = property(exported, "repaintScopes");
    REQUIRE((int64) property(repaintScopes, "canvas") == 1);
    REQUIRE((int64) property(repaintScopes, "status") == 1);
}

TEST_CASE("Canvas metrics aggregate presentation layer durations",
        "[cycle-v2][canvas][performance]") {
    fakeNow = 100;
    CanvasPerformanceMetrics metrics(fakeClock);

    {
        ScopedNodeCanvasPresentationStage measurement(
                &metrics,
                NodeCanvasPresentationStage::Nodes);
        fakeNow = 4300;
    }
    metrics.presentationStageCompleted(
            NodeCanvasPresentationStage::Nodes,
            1800);
    metrics.presentationStageCompleted(
            NodeCanvasPresentationStage::SpyRail,
            900);
    metrics.nodeLayerCacheCompleted(12, 2, 5800);
    metrics.nodeLayerCacheCompleted(14, 0, 220);

    const auto snapshot = metrics.snapshot();
    const auto& nodes = snapshot.presentationStages[static_cast<size_t>(
            NodeCanvasPresentationStage::Nodes)];
    REQUIRE(nodes.count == 2);
    REQUIRE(nodes.totalMicroseconds == 6000);

    const var exported = metrics.toVar({});
    const var& stages = property(exported, "presentationStages");
    REQUIRE((double) property(property(stages, "nodes"), "meanMs")
            == Catch::Approx(3.0));
    REQUIRE((double) property(property(stages, "spyRail"), "maxMs")
            == Catch::Approx(0.9));
    const var& nodeLayerCache = property(property(exported, "presentationCache"), "nodeLayer");
    REQUIRE((int64) property(nodeLayerCache, "hits") == 26);
    REQUIRE((int64) property(nodeLayerCache, "misses") == 2);
    REQUIRE((double) property(property(nodeLayerCache, "hitDuration"), "meanMs")
            == Catch::Approx(0.22));
    REQUIRE((double) property(property(nodeLayerCache, "missDuration"), "meanMs")
            == Catch::Approx(5.8));

    metrics.reset();
    for (const auto& stage : metrics.snapshot().presentationStages) {
        REQUIRE(stage.count == 0);
    }
    REQUIRE(metrics.snapshot().nodeLayerCacheHits == 0);
    REQUIRE(metrics.snapshot().nodeLayerCacheMisses == 0);
}

TEST_CASE("Canvas metrics expose hover churn and native Trimesh edit operations",
        "[cycle-v2][canvas][performance]") {
    fakeNow = 100;
    CanvasPerformanceMetrics metrics(fakeClock);

    metrics.recordOperation(CanvasPerformanceMetrics::Operation::HoverResolution, 240);
    metrics.recordHoverState(false, true);
    metrics.recordHoverState(false);
    metrics.recordHoverState(true);
    metrics.nodeEditorOperationCompleted(
            NodeEditorPerformanceOperation::TrimeshVertexUpdate,
            4200);

    const var exported = metrics.toVar({});
    const var& operations = property(exported, "operations");
    const var& hover = property(operations, "hoverResolution");
    const var& update = property(operations, "trimeshVertexUpdate");
    const var& hoverState = property(exported, "hoverState");

    REQUIRE((int64) property(hover, "count") == 1);
    REQUIRE((double) property(hover, "meanMs") == Catch::Approx(0.24));
    REQUIRE((int64) property(update, "count") == 1);
    REQUIRE((double) property(update, "meanMs") == Catch::Approx(4.2));
    REQUIRE((int64) property(hoverState, "changed") == 1);
    REQUIRE((int64) property(hoverState, "unchanged") == 2);
    REQUIRE((int64) property(hoverState, "occluded") == 1);
}

TEST_CASE("Presentation metrics distinguish causal preview stages and outcomes",
        "[cycle-v2][canvas][performance]") {
    fakeNow = 100;
    GraphPresentationPerformanceMetrics metrics(fakeClock);
    metrics.record(GraphPresentationPerformanceMetrics::Outcome::Requested);
    metrics.record(GraphPresentationPerformanceMetrics::Stage::QueueDelay, 5000);
    metrics.record(GraphPresentationPerformanceMetrics::Stage::PreviewAudio, 120000);
    metrics.record(GraphPresentationPerformanceMetrics::Stage::PreviewExtraction, 7000);
    metrics.record(GraphPresentationPerformanceMetrics::Stage::EndToEnd, 150000);
    metrics.record(GraphPresentationPerformanceMetrics::Outcome::Published);

    const var exported = metrics.toVar();
    const var& stages = property(exported, "stages");
    const var& outcomes = property(exported, "outcomes");
    const var& audio = property(stages, "previewAudio");
    const var& endToEnd = property(stages, "endToEnd");

    REQUIRE((int64) property(audio, "count") == 1);
    REQUIRE((double) property(audio, "maxMs") == Catch::Approx(120.0));
    REQUIRE((double) property(endToEnd, "meanMs") == Catch::Approx(150.0));
    REQUIRE((int64) property(outcomes, "requested") == 1);
    REQUIRE((int64) property(outcomes, "published") == 1);

    metrics.reset();
    const auto resetStages = metrics.stageSnapshot();
    const auto resetOutcomes = metrics.outcomeSnapshot();
    REQUIRE(resetStages[static_cast<size_t>(
            GraphPresentationPerformanceMetrics::Stage::PreviewAudio)].count == 0);
    REQUIRE(resetOutcomes[static_cast<size_t>(
            GraphPresentationPerformanceMetrics::Outcome::Requested)] == 0);
}

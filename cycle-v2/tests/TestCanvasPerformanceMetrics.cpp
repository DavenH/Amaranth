#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "UI/CanvasPerformanceMetrics.h"

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
}

#include <catch2/catch_test_macros.hpp>

#include "Runtime/OscillatorRegionPlanView.h"

using namespace CycleV2;

TEST_CASE("Oscillator region plan view owns structural membership",
        "[cycle-v2][runtime][oscillator-region][plan]") {
    GraphExecutionPlan plan;
    plan.steps.resize(3);
    plan.steps[1].inputs.push_back({
            "source", "out", "in", 0, 0, 0, 0,
            PortDomain::TimeSignal, ChannelLayout::Mono
    });
    OscillatorRegionPlan region;
    region.stepIndices = { 0, 1 };
    region.materializationStepIndex = 1;

    const OscillatorRegionPlanView view(plan, region);

    REQUIRE(view.isValid());
    REQUIRE(view.containsStep(0));
    REQUIRE(view.containsStep(1));
    REQUIRE_FALSE(view.containsStep(2));
    REQUIRE(view.inputForPort(plan.steps[1], 0) != nullptr);
    REQUIRE(view.inputComesFromRegion(plan.steps[1], 0));

    region.stepIndices.push_back(3);
    REQUIRE_FALSE(OscillatorRegionPlanView(plan, region).isValid());
}

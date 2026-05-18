#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphValidator.h"

#include <algorithm>

using namespace CycleV2;

TEST_CASE("Demo graph validates", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    REQUIRE(GraphValidator().isValid(graph));
}

TEST_CASE("Scratch ports require attachment routing", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addEdge({ "env", "env", "wave", "scratch", PortDomain::EnvelopeSignal, false });

    auto issues = GraphValidator().validate(graph);

    REQUIRE_FALSE(issues.empty());
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::ScratchPortRequiresAttachment;
            }));
}

TEST_CASE("Pitch cannot feed non voice-aware processors", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addEdge({ "voice", "pitch", "multiply", "audio", PortDomain::PitchSignal, false });

    auto issues = GraphValidator().validate(graph);

    REQUIRE_FALSE(issues.empty());
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::DomainMismatch
                    || issue.code == GraphValidationCode::PitchRequiresVoiceAwareDestination;
            }));
}

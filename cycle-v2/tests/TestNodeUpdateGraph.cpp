#include <catch2/catch_test_macros.hpp>

#include "../src/Runtime/NodeUpdateGraph.h"

#include <map>

using namespace CycleV2;

namespace {

GraphExecutionPlan causalDiamondPlan() {
    GraphExecutionPlan plan;
    plan.nodeOrder = { "root", "left", "right", "join" };
    plan.dependencyIndex.nodeIds = plan.nodeOrder;
    plan.dependencyIndex.dependents = {
        { 1, 2 },
        { 3 },
        { 3 },
        {}
    };
    return plan;
}

}

TEST_CASE("Causal update graph merges a diamond before executing", "[cycle-v2][runtime][causal]") {
    NodeUpdateGraph graph;
    const auto plan = causalDiamondPlan();
    const CausalUpdateRequest request {
        { 1, 1, EditPhase::Movement },
        {
            {
                "root", "root:curve", UpdateProduct::PreviewTraversal, 91,
                { { "root", "curve" } }, true
            }
        },
        {}
    };
    std::map<String, int> executions;

    const auto result = graph.execute(plan, request, [&](const auto& product) {
        ++executions[product.nodeId];
        return true;
    });

    REQUIRE_FALSE(result.invariantViolation);
    REQUIRE(executions["root"] == 1);
    REQUIRE(executions["left"] == 1);
    REQUIRE(executions["right"] == 1);
    REQUIRE(executions["join"] == 1);
    REQUIRE(graph.trace().count(
            1, "join", UpdateProduct::PreviewTraversal, UpdateTracePhase::Completed) == 1);
}

TEST_CASE("Causal update graph combines causes from atomic roots", "[cycle-v2][runtime][causal]") {
    NodeUpdateGraph graph;
    const auto plan = causalDiamondPlan();
    const CausalUpdateRequest request {
        { 2, 1, EditPhase::Movement },
        {
            {
                "left", "envelope:plane", UpdateProduct::ProbePreview, 11,
                { { "left", "red" } }, true
            },
            {
                "right", "envelope:plane", UpdateProduct::ProbePreview, 12,
                { { "right", "blue" } }, true
            }
        },
        {}
    };
    std::vector<UpdateCause> joinCauses;

    graph.execute(plan, request, [&](const auto& product) {
        if (product.nodeId == "join") {
            joinCauses = product.causes;
        }
        return true;
    });

    REQUIRE(joinCauses.size() == 2);
    REQUIRE(graph.trace().count(
            2, "join", UpdateProduct::ProbePreview, UpdateTracePhase::Completed) == 1);
}

TEST_CASE("Semantic edit gate rejects unchanged effective state", "[cycle-v2][runtime][causal]") {
    SemanticEditGate gate;
    gate.beginGesture();

    const auto first = gate.accept("ir:length", 2048);
    const auto unchanged = gate.accept("ir:length", 2048);
    const auto next = gate.accept("ir:length", 4096);

    REQUIRE(first.has_value());
    REQUIRE_FALSE(unchanged.has_value());
    REQUIRE(next.has_value());
    REQUIRE(next->editId == first->editId + 1);
}

TEST_CASE("Semantic edit gate accepts zero as an initial effective state",
        "[cycle-v2][runtime][causal]") {
    SemanticEditGate gate;

    const auto initial = gate.accept("parameter", 0);
    const auto repeated = gate.accept("parameter", 0);

    REQUIRE(initial.has_value());
    REQUIRE_FALSE(repeated.has_value());
}

TEST_CASE("Standalone commits do not leak their gesture into later movement",
        "[cycle-v2][runtime][causal]") {
    SemanticEditGate gate;

    const auto initialCommit = gate.accept("document", 1, EditPhase::Commit);
    const auto movement = gate.accept("document", 2, EditPhase::Movement);
    const auto movementCommit = gate.commit("document");

    REQUIRE(initialCommit.has_value());
    REQUIRE(movement.has_value());
    REQUIRE(movementCommit.isValid());
    REQUIRE(movement->gestureId != initialCommit->gestureId);
    REQUIRE(movementCommit.gestureId == movement->gestureId);
}

TEST_CASE("Commit reuses an already current live product", "[cycle-v2][runtime][causal]") {
    NodeUpdateGraph graph;
    const auto plan = causalDiamondPlan();
    int executions {};
    const ProductInvalidation invalidation {
        "root", "root:curve", UpdateProduct::PreviewTraversal, 72,
        { { "root", "curve" } }, true
    };

    graph.execute(
            plan,
            { { 20, 4, EditPhase::Movement }, { invalidation }, {} },
            [&](const auto&) {
                ++executions;
                return true;
            });
    graph.execute(
            plan,
            { { 21, 4, EditPhase::Commit }, { invalidation }, {} },
            [&](const auto&) {
                ++executions;
                return true;
            });

    REQUIRE(executions == 4);
    REQUIRE(graph.trace().count(
            21, "join", UpdateProduct::PreviewTraversal, UpdateTracePhase::AlreadyCurrent) == 1);
}

TEST_CASE("A duplicate product execution for one edit violates the invariant",
        "[cycle-v2][runtime][causal]") {
    NodeUpdateGraph graph;
    const auto plan = causalDiamondPlan();
    const CausalUpdateRequest request {
        { 31, 8, EditPhase::Movement },
        {
            {
                "root", "root:curve", UpdateProduct::LocalSlice, 101,
                { { "root", "curve" } }, false
            }
        },
        {}
    };

    REQUIRE_FALSE(graph.execute(plan, request, [](const auto&) { return true; }).invariantViolation);
    REQUIRE(graph.execute(plan, request, [](const auto&) { return true; }).invariantViolation);
    REQUIRE(graph.trace().count(
            31, "root", UpdateProduct::LocalSlice, UpdateTracePhase::InvariantViolation) == 1);
}

TEST_CASE("Same-source generations reject stale downstream publication",
        "[cycle-v2][runtime][causal]") {
    NodeUpdateGraph graph;
    graph.supersede("envelope:plane", UpdateProduct::ProbePreview, 4);
    REQUIRE(graph.isCurrent("envelope:plane", UpdateProduct::ProbePreview, 4));

    graph.supersede("envelope:plane", UpdateProduct::ProbePreview, 5);

    REQUIRE_FALSE(graph.isCurrent("envelope:plane", UpdateProduct::ProbePreview, 4));
    REQUIRE(graph.isCurrent("envelope:plane", UpdateProduct::ProbePreview, 5));
    REQUIRE(graph.isCurrent("other-editor", UpdateProduct::ProbePreview, 1));
}

TEST_CASE("Deferred causal execution publishes only after derived work succeeds",
        "[cycle-v2][runtime][causal]") {
    NodeUpdateGraph graph;
    const auto plan = causalDiamondPlan();
    const CausalUpdateRequest request {
        { 41, 9, EditPhase::Movement },
        {
            {
                "root", "editor:root", UpdateProduct::PreviewTraversal, 501,
                { { "root", "curve" } }, true
            }
        },
        {}
    };
    std::vector<String> processedNodes;

    const auto result = graph.executeDeferredPublication(
            plan,
            request,
            [&](const auto& products) {
                for (const auto& product : products) {
                    processedNodes.push_back(product.nodeId);
                }
                return true;
            });

    REQUIRE(processedNodes == plan.nodeOrder);
    REQUIRE(graph.trace().count(
            41, "join", UpdateProduct::PreviewTraversal, UpdateTracePhase::Completed) == 1);
    REQUIRE(graph.trace().count(
            41, "join", UpdateProduct::PreviewTraversal, UpdateTracePhase::Published) == 0);

    graph.publish(request, result);

    REQUIRE(graph.trace().count(
            41, "join", UpdateProduct::PreviewTraversal, UpdateTracePhase::Published) == 1);
}

TEST_CASE("Probe planning excludes nodes without active observations",
        "[cycle-v2][runtime][causal]") {
    NodeUpdateGraph graph;
    const auto plan = causalDiamondPlan();
    const CausalUpdateRequest request {
        { 42, 10, EditPhase::Movement },
        {
            {
                "root", "editor:root", UpdateProduct::ProbePreview, 502,
                { { "root", "curve" } }, true
            }
        },
        { "left" },
        true
    };

    const auto result = graph.execute(plan, request, [](const auto&) { return true; });

    REQUIRE(result.executed.size() == 1);
    REQUIRE(result.executed.front().nodeId == "left");
    REQUIRE(graph.trace().count(
            42, "join", UpdateProduct::ProbePreview, UpdateTracePhase::NotObserved) == 1);
}

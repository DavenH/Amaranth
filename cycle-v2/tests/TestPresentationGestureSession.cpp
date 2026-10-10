#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphNodeFactory.h"
#include "Graph/NodeParameterMap.h"
#include "Runtime/PresentationGestureSession.h"

using namespace CycleV2;

TEST_CASE("Presentation gesture session retains movement identity and commits once",
        "[cycle-v2][runtime][presentation-session]") {
    PresentationGestureSession session;
    const auto first = session.recordMovement("editor:mesh", 10);
    REQUIRE(first.has_value());
    REQUIRE(session.activeStreamOr("graph") == "editor:mesh");
    const auto requestedFirst = session.identityForRequest(
            "editor:mesh", 10, EditPhase::Movement);
    REQUIRE(requestedFirst.has_value());
    REQUIRE(requestedFirst->editId == first->editId);

    const auto second = session.recordMovement("editor:mesh", 11);
    REQUIRE(second.has_value());
    REQUIRE(second->gestureId == first->gestureId);
    REQUIRE(second->editId != first->editId);
    const auto requestedSecond = session.identityForRequest(
            "editor:mesh", 11, EditPhase::Movement);
    REQUIRE(requestedSecond.has_value());
    REQUIRE(requestedSecond->editId == second->editId);

    const auto committed = session.identityForRequest(
            "editor:mesh", 11, EditPhase::Commit);
    REQUIRE(committed.has_value());
    REQUIRE(committed->phase == EditPhase::Commit);
    REQUIRE(committed->gestureId == first->gestureId);
    REQUIRE(session.activeStreamOr("graph") == "graph");
}

TEST_CASE("Presentation gesture session keeps source streams independent",
        "[cycle-v2][runtime][presentation-session]") {
    PresentationGestureSession session;
    const auto movement = session.recordMovement("editor:first", 10);
    REQUIRE(movement.has_value());
    const auto other = session.identityForRequest(
            "editor:second", 20, EditPhase::Movement);
    REQUIRE(other.has_value());
    REQUIRE(other->editId != movement->editId);
    REQUIRE(session.activeStreamOr("graph") == "editor:first");
    const auto requestedMovement = session.identityForRequest(
            "editor:first", 10, EditPhase::Movement);
    REQUIRE(requestedMovement.has_value());
    REQUIRE(requestedMovement->editId == movement->editId);
}

TEST_CASE("Presentation gesture session preserves pending identities for concurrent streams",
        "[cycle-v2][runtime][presentation-session]") {
    PresentationGestureSession session;
    const auto first = session.recordMovement("editor:first", 10);
    const auto second = session.recordMovement("editor:second", 20);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());

    const auto requestedFirst = session.identityForRequest(
            "editor:first", 10, EditPhase::Movement);
    const auto requestedSecond = session.identityForRequest(
            "editor:second", 20, EditPhase::Movement);
    REQUIRE(requestedFirst.has_value());
    REQUIRE(requestedSecond.has_value());
    REQUIRE(requestedFirst->editId == first->editId);
    REQUIRE(requestedSecond->editId == second->editId);

    session.commit("editor:first");
    REQUIRE(session.activeStreamOr("graph") == "editor:second");
    session.cancel("editor:second");
    REQUIRE(session.activeStreamOr("graph") == "graph");

    REQUIRE(session.recordMovement("editor:first", 11).has_value());
    REQUIRE(session.recordMovement("editor:second", 21).has_value());
    session.cancel("editor:second");
    REQUIRE(session.activeStreamOr("graph") == "editor:first");
}

TEST_CASE("Presentation gesture cancellation restores the prior effective state",
        "[cycle-v2][runtime][presentation-session]") {
    PresentationGestureSession session;
    REQUIRE(session.identityForRequest(
            "editor:mesh", 10, EditPhase::Commit).has_value());

    const auto movement = session.recordMovement("editor:mesh", 11);
    REQUIRE(movement.has_value());
    session.cancel("editor:mesh");
    REQUIRE(session.activeStreamOr("graph") == "graph");

    const auto retriedMovement = session.recordMovement("editor:mesh", 11);
    REQUIRE(retriedMovement.has_value());
    REQUIRE(retriedMovement->editId != movement->editId);
    REQUIRE(session.commit("editor:mesh").isValid());
}

TEST_CASE("Presentation gesture session owns transient graph lifecycle",
        "[cycle-v2][runtime][presentation-session][gesture]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    PresentationGestureSession session;
    const uint64_t baseRevision = document.revision();

    REQUIRE(session.beginGraphGesture(
            "preview:wheel", commands, document, ProbeRefreshMode::LiveLatest, 0));
    REQUIRE(commands.hasTransientEdit());
    const auto first = session.recordGraphMovement("preview:wheel", 32);
    REQUIRE(first.has_value());
    REQUIRE(commands.setPreviewMorph(0.25f, 32.f / 127.f).changed);
    const auto firstSnapshot = session.snapshotGraphGesture("preview:wheel", commands);
    REQUIRE(firstSnapshot != nullptr);

    const auto second = session.recordGraphMovement("preview:wheel", 96);
    REQUIRE(second.has_value());
    REQUIRE(second->gestureId == first->gestureId);
    REQUIRE(commands.setPreviewMorph(0.25f, 96.f / 127.f).changed);
    const auto finalSnapshot = session.snapshotGraphGesture("preview:wheel", commands);
    REQUIRE(finalSnapshot != nullptr);
    REQUIRE(document.revision() == baseRevision);
    REQUIRE(NodeParameterMap(*firstSnapshot->findNode("mesh")).floatValue("blue")
            == 32.f / 127.f);

    const auto finished = session.finishGraphGesture("preview:wheel", commands, document);
    REQUIRE(finished.live);
    REQUIRE(finished.changed);
    REQUIRE(finished.durableChanged);
    REQUIRE(finished.finalSnapshot == finalSnapshot);
    REQUIRE_FALSE(commands.hasTransientEdit());
    REQUIRE(document.revision() > baseRevision);
    REQUIRE(document.undo());
    REQUIRE(NodeParameterMap(*finalSnapshot->findNode("mesh")).floatValue("blue")
            == 96.f / 127.f);
}

TEST_CASE("On Release presentation gesture defers durable morph state",
        "[cycle-v2][runtime][presentation-session][gesture]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    PresentationGestureSession session;
    const uint64_t baseRevision = document.revision();

    REQUIRE(session.beginGraphGesture(
            "preview:wheel", commands, document, ProbeRefreshMode::OnGestureCommit, 0));
    REQUIRE_FALSE(commands.hasTransientEdit());
    REQUIRE(session.recordGraphMovement("preview:wheel", 32).has_value());
    REQUIRE(session.recordGraphMovement("preview:wheel", 96).has_value());
    REQUIRE(session.snapshotGraphGesture("preview:wheel", commands) == nullptr);
    REQUIRE(document.revision() == baseRevision);

    const auto finished = session.finishGraphGesture("preview:wheel", commands, document);
    REQUIRE_FALSE(finished.live);
    REQUIRE(finished.changed);
    REQUIRE_FALSE(finished.durableChanged);
    REQUIRE(commands.setPreviewMorph(0.25f, 96.f / 127.f).changed);
    REQUIRE(document.revision() > baseRevision);
    REQUIRE(document.undo());
}

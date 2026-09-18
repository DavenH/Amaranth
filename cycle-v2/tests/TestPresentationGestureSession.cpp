#include <catch2/catch_test_macros.hpp>

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

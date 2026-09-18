#include <catch2/catch_test_macros.hpp>

#include "Runtime/PresentationRefreshPolicy.h"

using namespace CycleV2;

TEST_CASE("Presentation refresh policy keeps local movement and defers On Release probes",
        "[cycle-v2][runtime][presentation-policy]") {
    const PresentationRefreshDecision movement = PresentationRefreshPolicy::decide({
            EditPhase::Movement,
            ProbeRefreshMode::OnGestureCommit,
            UpdateProduct::LocalSlice,
            true,
            false
    });
    REQUIRE(movement.localProduct == UpdateProduct::LocalSlice);
    REQUIRE(movement.downstream == DownstreamRefresh::None);

    const PresentationRefreshDecision commit = PresentationRefreshPolicy::decide({
            EditPhase::Commit,
            ProbeRefreshMode::OnGestureCommit,
            UpdateProduct::LocalSlice,
            true,
            false
    });
    REQUIRE_FALSE(commit.localProduct.has_value());
    REQUIRE(commit.downstream == DownstreamRefresh::CommitAsync);
}

TEST_CASE("Presentation refresh policy reuses a published Live result on commit",
        "[cycle-v2][runtime][presentation-policy]") {
    const PresentationRefreshDecision movement = PresentationRefreshPolicy::decide({
            EditPhase::Movement,
            ProbeRefreshMode::LiveLatest,
            UpdateProduct::LocalSurface,
            true,
            false
    });
    REQUIRE(movement.localProduct == UpdateProduct::LocalSurface);
    REQUIRE(movement.downstream == DownstreamRefresh::LatestAsync);

    const PresentationRefreshDecision commit = PresentationRefreshPolicy::decide({
            EditPhase::Commit,
            ProbeRefreshMode::LiveLatest,
            UpdateProduct::LocalSurface,
            true,
            true
    });
    REQUIRE_FALSE(commit.localProduct.has_value());
    REQUIRE(commit.downstream == DownstreamRefresh::ReuseLatest);

    const PresentationRefreshDecision unfinished = PresentationRefreshPolicy::decide({
            EditPhase::Commit,
            ProbeRefreshMode::LiveLatest,
            UpdateProduct::LocalSurface,
            true,
            false
    });
    REQUIRE(unfinished.downstream == DownstreamRefresh::CommitAsync);
}

TEST_CASE("Presentation refresh policy skips downstream work for a local-only edit",
        "[cycle-v2][runtime][presentation-policy]") {
    for (const ProbeRefreshMode mode : {
            ProbeRefreshMode::OnGestureCommit,
            ProbeRefreshMode::LiveLatest }) {
        const PresentationRefreshDecision movement = PresentationRefreshPolicy::decide({
                EditPhase::Movement,
                mode,
                UpdateProduct::LocalSlice,
                false,
                false
        });
        REQUIRE(movement.localProduct == UpdateProduct::LocalSlice);
        REQUIRE(movement.downstream == DownstreamRefresh::None);

        const PresentationRefreshDecision commit = PresentationRefreshPolicy::decide({
                EditPhase::Commit,
                mode,
                UpdateProduct::LocalSlice,
                false,
                false
        });
        REQUIRE_FALSE(commit.localProduct.has_value());
        REQUIRE(commit.downstream == DownstreamRefresh::None);
    }
}

#include "Runtime/PresentationRefreshPolicy.h"

namespace CycleV2 {

PresentationRefreshDecision PresentationRefreshPolicy::decide(
        const PresentationEditContext& context) {
    PresentationRefreshDecision decision;
    if (context.phase == EditPhase::Movement) {
        decision.localProduct = context.localProduct;
    }
    if (!context.downstreamChanged) {
        return decision;
    }
    if (context.phase == EditPhase::Movement) {
        if (context.probeMode == ProbeRefreshMode::LiveLatest) {
            decision.downstream = DownstreamRefresh::LatestAsync;
        }
        return decision;
    }
    if (context.probeMode == ProbeRefreshMode::LiveLatest
            && context.finalMovementAlreadyPublished) {
        decision.downstream = DownstreamRefresh::ReuseLatest;
    } else {
        decision.downstream = DownstreamRefresh::CommitAsync;
    }
    return decision;
}

bool PresentationRefreshPolicy::schedulesDownstreamDuringMovement(
        ProbeRefreshMode mode) {
    return decide({
            EditPhase::Movement,
            mode,
            std::nullopt,
            true,
            false
    }).downstream == DownstreamRefresh::LatestAsync;
}

}

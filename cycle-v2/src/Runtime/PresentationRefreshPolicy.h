#pragma once

#include <optional>

#include "Runtime/NodeUpdateGraph.h"

namespace CycleV2 {

enum class ProbeRefreshMode {
    OnGestureCommit,
    LiveLatest
};

enum class DownstreamRefresh {
    None,
    LatestAsync,
    CommitAsync,
    ReuseLatest
};

struct PresentationEditContext {
    EditPhase phase { EditPhase::Movement };
    ProbeRefreshMode probeMode { ProbeRefreshMode::OnGestureCommit };
    std::optional<UpdateProduct> localProduct;
    bool downstreamChanged {};
    bool finalMovementAlreadyPublished {};
};

struct PresentationRefreshDecision {
    std::optional<UpdateProduct> localProduct;
    DownstreamRefresh downstream { DownstreamRefresh::None };
};

class PresentationRefreshPolicy final {
public:
    static PresentationRefreshDecision decide(const PresentationEditContext& context);
};

}

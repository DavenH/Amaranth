#pragma once

#include "../Policies/Core/SnapshotPolicy.h"

namespace Rasterization {
    template<class Policy = RasterizerDataSnapshot>
    class RasterizerSnapshotBuilder {
    public:
        explicit RasterizerSnapshotBuilder(Policy policy = Policy()) :
                policy(policy) {
        }

        void publish(RasterizerData& target, const RasterizerSnapshotSource& source) const {
            policy.publish(target, source);
        }

    private:
        Policy policy;
    };
}

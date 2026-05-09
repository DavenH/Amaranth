#pragma once

#include <algorithm>
#include <vector>

#include "../../../Intercept.h"
#include "../../../../Util/Util.h"

namespace Rasterization {
    class InterceptSortPolicy {
    public:
        explicit InterceptSortPolicy(bool* needsResorting) :
                needsResorting(needsResorting) {
        }

        void sortIfNeeded(std::vector<Intercept>& intercepts) const {
            if (needsResorting == nullptr) {
                return;
            }

            if (Util::assignAndWereDifferent(*needsResorting, false)) {
                std::sort(intercepts.begin(), intercepts.end());
            }
        }

    private:
        bool* needsResorting {};
    };
}

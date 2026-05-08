#pragma once

#include <cstddef>

namespace Rasterization {
    enum class InterceptDegeneracyAction {
        Continue,
        CleanUp,
        MarkUnsampleable
    };

    class InterceptDegeneracyPolicy {
    public:
        InterceptDegeneracyAction classify(std::size_t interceptCount) const {
            if (interceptCount == 0) {
                return InterceptDegeneracyAction::CleanUp;
            }

            if (interceptCount == 1) {
                return InterceptDegeneracyAction::MarkUnsampleable;
            }

            return InterceptDegeneracyAction::Continue;
        }
    };
}

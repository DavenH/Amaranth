#pragma once

#include <vector>

#include "../../Intercept.h"

namespace Rasterization {
    class InterceptPaddingFlagPolicy {
    public:
        void apply(std::vector<Intercept>& intercepts) const {
            bool padAny = false;

            for (int i = 0; i < (int) intercepts.size() - 1; ++i) {
                Intercept& current = intercepts[i];
                Intercept& next = intercepts[i + 1];
                bool pad = current.cube != nullptr && current.cube->getCompGuideCurve() >= 0;
                current.padBefore = pad;
                next.padAfter = pad;

                padAny |= pad;
            }

            if (!padAny) {
                return;
            }

            for (int i = 1; i < (int) intercepts.size(); ++i) {
                intercepts[i].padBefore &= !intercepts[i - 1].padBefore;
            }

            for (int i = 0; i < (int) intercepts.size() - 1; ++i) {
                intercepts[i].padAfter &= !intercepts[i + 1].padAfter;
            }
        }
    };
}

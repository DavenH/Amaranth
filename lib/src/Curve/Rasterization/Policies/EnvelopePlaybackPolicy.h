#pragma once

#include <vector>

#include "../../Intercept.h"

namespace Rasterization {
    struct EnvelopePlaybackContext {
        int loopIndex { -1 };
        int sustainIndex { -1 };
        bool releasing {};
    };

    class EnvelopePlaybackPolicy {
    public:
        bool hasReleaseCurve(
                const std::vector<Intercept>& intercepts,
                int sustainIndex) const {
            if (intercepts.empty()) {
                return false;
            }

            return sustainIndex != (int) intercepts.size() - 1;
        }

        float loopLength(
                const std::vector<Intercept>& intercepts,
                const EnvelopePlaybackContext& context) const {
            if (context.loopIndex >= 0
                && context.loopIndex < (int) intercepts.size() - 1
                && context.sustainIndex >= 0
                && context.sustainIndex < (int) intercepts.size()) {
                return intercepts[context.sustainIndex].x - intercepts[context.loopIndex].x;
            }

            return -1.f;
        }

        double boundary(
                const std::vector<Intercept>& intercepts,
                const EnvelopePlaybackContext& context) const {
            if (intercepts.empty()) {
                return 0.;
            }

            if (context.releasing) {
                return intercepts.back().x;
            }

            if (context.sustainIndex >= 0 && context.sustainIndex < (int) intercepts.size()) {
                return intercepts[context.sustainIndex].x;
            }

            return intercepts.back().x;
        }
    };
}

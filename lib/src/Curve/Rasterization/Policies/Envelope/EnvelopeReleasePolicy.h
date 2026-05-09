#pragma once

#include <vector>

#include "../../../Intercept.h"

namespace Rasterization {
    struct EnvelopeReleaseContext {
        bool bipolar {};
        int sustainIndex { -1 };
    };

    struct EnvelopeReleaseStart {
        int index { -1 };
        float position {};
        float scale { 1.f };
    };

    class EnvelopeReleasePolicy {
    public:
        int releaseIndex(const EnvelopeReleaseContext& context) const {
            return context.bipolar ? context.sustainIndex : context.sustainIndex + 1;
        }

        EnvelopeReleaseStart start(
                const std::vector<Intercept>& intercepts,
                const EnvelopeReleaseContext& context,
                float sustainLevel,
                float sampledReleaseLevel) const {
            EnvelopeReleaseStart result;
            result.index = releaseIndex(context);

            if (result.index < 0 || result.index >= (int) intercepts.size()) {
                return result;
            }

            result.position = intercepts[result.index].x;

            float initialReleaseLevel = sampledReleaseLevel < 0.5f ? 0.5f : sampledReleaseLevel;
            result.scale = sustainLevel / initialReleaseLevel;

            return result;
        }
    };
}

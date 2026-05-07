#pragma once

#include "../Policies/EnvelopeMarkerPolicy.h"
#include "../Policies/EnvelopePaddingPolicy.h"
#include "../Policies/EnvelopeSustainPointPolicy.h"

namespace Rasterization {
    class EnvRasterizerFacade {
    public:
        EnvelopeMarkerResult evaluateMarkers(
                const std::vector<Intercept>& intercepts,
                const EnvelopeMesh* mesh,
                int loopMinSizeIcpts) const {
            return markerPolicy.evaluate(intercepts, mesh, loopMinSizeIcpts);
        }

        bool applySustainPoint(std::vector<Intercept>& intercepts, const EnvelopeSustainPointContext& context) const {
            return sustainPointPolicy.apply(intercepts, context);
        }

        bool buildDisplayPadding(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves,
                const EnvelopePaddingContext& context) const {
            return paddingPolicy.buildDisplayPadding(intercepts, curves, context);
        }

        bool buildRenderPadding(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves,
                const EnvelopePaddingContext& context) const {
            return paddingPolicy.buildRenderPadding(intercepts, curves, context);
        }

    private:
        EnvelopeMarkerPolicy markerPolicy;
        EnvelopePaddingPolicy paddingPolicy;
        EnvelopeSustainPointPolicy sustainPointPolicy;
    };
}

#pragma once

#include <vector>

#include "../../../Intercept.h"
#include "../../../VertCube.h"
#include "../../../../Inter/Dimensions.h"
#include "../../../../Obj/ColorPoint.h"
#include "../../../../Obj/MorphPosition.h"

namespace Rasterization {
    class DepthProjectionPolicy {
    public:
        struct Context {
            bool enabled {};
            bool cyclic {};
            int visibleDimension {};
            int sliceDimension {};
            float independent {};
            float oscPhase {};
            Dimensions dims;
            MorphPosition morph;
        };

        explicit DepthProjectionPolicy(bool enabled = false) :
                enabled(enabled) {
        }

        template<typename GuideApplier>
        void project(
                const Context& context,
                VertCube* cube,
                VertCube::ReductionData& reduction,
                const Intercept& midIntercept,
                GuideApplier&& applyGuide,
                std::vector<ColorPoint>& colorPoints) const {
            if (!enabled || !context.enabled || cube == nullptr) {
                return;
            }

            Intercept adjustedMidIntercept = midIntercept;
            adjustedMidIntercept.cube = cube;
            adjustedMidIntercept.adjustedX = adjustedMidIntercept.x;
            applyGuide(adjustedMidIntercept, context.morph, false);

            for (int i = 0; i < context.dims.numHidden(); ++i) {
                appendHiddenDimension(
                        context,
                        cube,
                        reduction,
                        applyGuide,
                        colorPoints,
                        adjustedMidIntercept,
                        context.dims.hidden[i]);
            }
        }

        bool isEnabled() const { return enabled; }

    private:
        template<typename GuideApplier>
        static void appendHiddenDimension(
                const Context& context,
                VertCube* cube,
                VertCube::ReductionData& reduction,
                GuideApplier&& applyGuide,
                std::vector<ColorPoint>& colorPoints,
                const Intercept& midIntercept,
                int hiddenDimension) {
            const int independentDims[] = { Vertex::Time, Vertex::Red, Vertex::Blue };
            float poleA[3];
            float poleB[3];

            for (int i = 0; i < 3; ++i) {
                poleA[i] = hiddenDimension == independentDims[i] ? 0.f : context.morph[independentDims[i]];
                poleB[i] = hiddenDimension == independentDims[i] ? 1.f : context.morph[independentDims[i]];
            }

            MorphPosition posA(poleA[0], poleA[1], poleA[2]);
            MorphPosition posB(poleB[0], poleB[1], poleB[2]);

            Intercept beforeIntercept = createPoleIntercept(context, cube, reduction, posA);
            applyGuide(beforeIntercept, posA, hiddenDimension == context.visibleDimension);

            Intercept afterIntercept = createPoleIntercept(context, cube, reduction, posB);
            applyGuide(afterIntercept, posB, hiddenDimension == context.visibleDimension);

            appendColorPoints(
                    context,
                    cube,
                    Vertex2(beforeIntercept.adjustedX, beforeIntercept.y),
                    Vertex2(midIntercept.adjustedX, midIntercept.y),
                    Vertex2(afterIntercept.adjustedX, afterIntercept.y),
                    hiddenDimension,
                    colorPoints);
        }

        static Intercept createPoleIntercept(
                const Context& context,
                VertCube* cube,
                VertCube::ReductionData& reduction,
                const MorphPosition& position) {
            cube->getFinalIntercept(reduction, position);

            Intercept intercept(
                    reduction.v.values[context.dims.x],
                    reduction.v.values[context.dims.y],
                    cube);
            intercept.adjustedX = intercept.x;

            return intercept;
        }

        static void appendColorPoints(
                const Context& context,
                VertCube* cube,
                Vertex2 before,
                Vertex2 mid,
                Vertex2 after,
                int hiddenDimension,
                std::vector<ColorPoint>& colorPoints) {
            if (context.cyclic) {
                if ((before.x > 1) != (after.x > 1)) {
                    colorPoints.emplace_back(
                            cube,
                            Vertex2(before.x - 1.f, before.y),
                            Vertex2(mid.x - 1.f, mid.y),
                            Vertex2(after.x - 1.f, after.y),
                            hiddenDimension);
                }

                if (before.x > 1 && after.x > 1) {
                    before.x -= 1.f;
                    mid.x    -= 1.f;
                    after.x  -= 1.f;
                }
            }

            colorPoints.emplace_back(cube, before, mid, after, hiddenDimension);
        }

        bool enabled;
    };
}

#pragma once

#include <algorithm>
#include <vector>

#include "../Policies/DefaultVertexWrapPolicy.h"
#include "../Policies/DepthProjectionPolicy.h"
#include "../Policies/InterceptRestrictionPolicy.h"
#include "../Policies/PointScalingPolicy.h"
#include "../RasterizationRequest.h"
#include "../Sources/MeshCubeSource.h"
#include "../../VertCube.h"
#include "../../../Obj/ColorPoint.h"
#include "../../../Util/NumberUtils.h"

namespace Rasterization {
    class MeshSlicePipeline {
    public:
        struct Output {
            std::vector<Intercept> intercepts;
            std::vector<ColorPoint> colorPoints;
            bool needsResort {};
            bool sampleable {};
        };

        static void applyPaddingFlags(std::vector<Intercept>& intercepts) {
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

        template<typename GuideApplier>
        const Output& render(
                const MeshCubeSource& source,
                const RasterizationRequest& request,
                float oscPhase,
                GuideApplier&& applyGuide) {
            return renderWithReduction(source, request, oscPhase, applyGuide, reduction);
        }

        template<typename GuideApplier>
        const Output& renderWithReduction(
                const MeshCubeSource& source,
                const RasterizationRequest& request,
                float oscPhase,
                GuideApplier&& applyGuide,
                VertCube::ReductionData& reductionData) {
            output = Output();

            if (source.empty()) {
                return output;
            }

            int sliceDimension = request.overrideDimension
                               ? request.overridingDimension
                               : request.primaryViewDimension;
            float independent = independentValue(sliceDimension, request.morph);
            DefaultVertexWrapPolicy wrapPolicy(request.cyclic);
            PointScalingPolicy pointScaling(request.scalingMode);

            for (int i = 0; i < source.size(); ++i) {
                appendCubeIntercept(
                        source.cubeAt(i),
                        sliceDimension,
                        independent,
                        oscPhase,
                        request,
                        pointScaling,
                        wrapPolicy,
                        applyGuide,
                        reductionData);
            }

            std::sort(output.intercepts.begin(), output.intercepts.end());
            restrict(output.intercepts, request);

            applyPaddingFlags(output.intercepts);
            output.sampleable = output.intercepts.size() >= 2;

            return output;
        }

    private:
        static float independentValue(int dimension, const MorphPosition& morph) {
            return dimension == Vertex::Time ? morph.time :
                   dimension == Vertex::Red  ? morph.red  :
                                                morph.blue;
        }

        template<typename GuideApplier>
        void appendCubeIntercept(
                VertCube* cube,
                int sliceDimension,
                float independent,
                float oscPhase,
                const RasterizationRequest& request,
                const PointScalingPolicy& pointScaling,
                const DefaultVertexWrapPolicy& wrapPolicy,
                GuideApplier&& applyGuide,
                VertCube::ReductionData& reductionData) {
            cube->getInterceptsFast(sliceDimension, reductionData, request.morph);
            if (!reductionData.pointOverlaps) {
                return;
            }

            Vertex* a = &reductionData.v0;
            Vertex* b = &reductionData.v1;
            Vertex* vertex = &reductionData.v;
            Intercept midIntercept;

            if (request.calcDepthDimensions) {
                VertCube::vertexAt(independent, sliceDimension, a, b, vertex);
                midIntercept.x = vertex->values[Vertex::Phase] + oscPhase;
                midIntercept.y = vertex->values[Vertex::Amp];
            }

            wrapPolicy.wrap(
                    a->values[sliceDimension],
                    a->values[Vertex::Phase],
                    b->values[sliceDimension],
                    b->values[Vertex::Phase],
                    independent);

            VertCube::vertexAt(independent, sliceDimension, a, b, vertex);

            Intercept intercept = makeIntercept(vertex, cube, oscPhase, request, pointScaling);
            applyGuide(intercept, request.morph, false);
            output.intercepts.emplace_back(intercept);

            projectDepth(
                    request,
                    cube,
                    sliceDimension,
                    independent,
                    oscPhase,
                    midIntercept,
                    applyGuide,
                    reductionData);
        }

        static Intercept makeIntercept(
                Vertex* vertex,
                VertCube* cube,
                float oscPhase,
                const RasterizationRequest& request,
                const PointScalingPolicy& pointScaling) {
            float x = vertex->values[Vertex::Phase] + oscPhase;

            if (request.cyclic) {
                while (x >= 1.f) {
                    x -= 1.f;
                }
                while (x < 0.f) {
                    x += 1.f;
                }
            } else {
                NumberUtils::constrain(x, request.xMinimum, request.xMaximum);
            }

            Intercept intercept(x, pointScaling.scale(vertex->values[Vertex::Amp]), cube);
            intercept.shp = vertex->values[Vertex::Curve];
            intercept.adjustedX = intercept.x;

            if (!(intercept.y == intercept.y)) {
                intercept.y = 0.5f;
            }

            return intercept;
        }

        template<typename GuideApplier>
        void projectDepth(
                const RasterizationRequest& request,
                VertCube* cube,
                int sliceDimension,
                float independent,
                float oscPhase,
                const Intercept& midIntercept,
                GuideApplier&& applyGuide,
                VertCube::ReductionData& reductionData) {
            DepthProjectionPolicy::Context context;
            context.enabled          = request.calcDepthDimensions;
            context.cyclic           = request.cyclic;
            context.visibleDimension = request.primaryViewDimension;
            context.sliceDimension   = sliceDimension;
            context.independent      = independent;
            context.oscPhase         = oscPhase;
            context.dims             = request.dims;
            context.morph            = request.morph;

            DepthProjectionPolicy(request.calcDepthDimensions).project(
                    context,
                    cube,
                    reductionData,
                    midIntercept,
                    applyGuide,
                    output.colorPoints);
        }

        void restrict(std::vector<Intercept>& intercepts, const RasterizationRequest& request) {
            InterceptRestrictionPolicy::Context context;
            context.cyclic = request.cyclic;
            context.minimumX = request.xMinimum;
            context.maximumX = request.xMaximum;

            InterceptRestrictionPolicy(context).restrict(intercepts);
        }

        Output output;
        VertCube::ReductionData reduction;
    };
}

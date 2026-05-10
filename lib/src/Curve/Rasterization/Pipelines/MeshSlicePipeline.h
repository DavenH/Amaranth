#pragma once

#include <algorithm>
#include <vector>

#include "../Policies/Mesh/DepthProjectionPolicy.h"
#include "../Policies/Curves/CurvePolicies.h"
#include "../Policies/Core/InterceptPolicies.h"
#include "../Policies/Core/PointScalingPolicy.h"
#include "../RasterizationRequest.h"
#include "../RenderResult.h"
#include "../Interpolation/TrilinearMeshSlicer.h"
#include "../../Mesh.h"
#include "../../VertCube.h"
#include "../../../Obj/ColorPoint.h"
#include "../../../Util/NumberUtils.h"

namespace Rasterization {
    class MeshSlicePipeline {
    public:
        template<typename GuideApplier>
        const RenderResult& render(
                Mesh* mesh,
                const TrilinearMeshSlicer& slicer,
                const RasterizationRequest& request,
                float oscPhase,
                GuideApplier&& applyGuide) {
            return renderWithReduction(mesh, slicer, request, oscPhase, applyGuide, reduction);
        }

        template<typename GuideApplier>
        const RenderResult& renderWithReduction(
                Mesh* mesh,
                const TrilinearMeshSlicer& slicer,
                const RasterizationRequest& request,
                float oscPhase,
                GuideApplier&& applyGuide,
                VertCube::ReductionData& reductionData) {
            output.clear();

            if (mesh == nullptr || mesh->getNumCubes() == 0) {
                return output;
            }

            int sliceDimension = request.overrideDimension
                               ? request.overridingDimension
                               : request.primaryViewDimension;
            float independent = independentValue(sliceDimension, request.morph);
            PointScalingPolicy pointScaling(request.scalingMode);

            auto& cubes = mesh->getCubes();
            for (int i = 0; i < (int) cubes.size(); ++i) {
                appendCubeIntercept(
                        cubes[i],
                        slicer,
                        sliceDimension,
                        independent,
                        oscPhase,
                        request,
                        pointScaling,
                        applyGuide,
                        reductionData);
            }

            std::sort(output.intercepts.begin(), output.intercepts.end());
            restrict(output.intercepts, request);

            InterceptPaddingFlagPolicy().apply(output.intercepts);
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
                const TrilinearMeshSlicer& slicer,
                int sliceDimension,
                float independent,
                float oscPhase,
                const RasterizationRequest& request,
                const PointScalingPolicy& pointScaling,
                GuideApplier&& applyGuide,
                VertCube::ReductionData& reductionData) {
            slicer.slice(*cube, sliceDimension, reductionData, request.morph);
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

            wrapVertexPair(
                    request.cyclic,
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

        static void wrapVertexPair(
                bool cyclic,
                float& ax,
                float& ay,
                float& bx,
                float& by,
                float independent) {
            if (!cyclic) {
                return;
            }

            if (ay > 1.f && by > 1.f) {
                ay -= 1.f;
                by -= 1.f;
            } else if ((ay > 1.f) != (by > 1.f)) {
                float intercept = ax + (1.f - ay) / ((ay - by) / (ax - bx));
                if (intercept > independent) {
                    ay -= 1.f;
                    by -= 1.f;
                }
            }
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

        RenderResult output;
        VertCube::ReductionData reduction;
    };
}

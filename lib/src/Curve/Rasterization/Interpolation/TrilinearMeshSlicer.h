#pragma once

#include <algorithm>
#include <Curve/Mesh/MorphDimensions.h>
#include <Curve/Rasterization/PreparedMeshSlice.h>
#include <utility>
#include <vector>

#include "../Policies/Core/InterceptPolicies.h"
#include "../Policies/Core/PointScalingPolicy.h"
#include "../Policies/Curves/CurvePolicies.h"
#include "../Policies/Mesh/DepthProjectionPolicy.h"
#include "../RasterizationRequest.h"
#include "../RenderResult.h"
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Mesh/Vertex2.h>
#include "../../../Obj/MorphPosition.h"
#include "../../../Obj/ColorPoint.h"
#include "../../../Util/NumberUtils.h"

namespace Rasterization {
    class TrilinearMeshSlicer {
    public:
        constexpr TrilinearMeshSlicer() = default;

        bool slice(
                const VertCube& cube,
                int dimension,
                VertCube::ReductionData& data,
                const MorphPosition& position,
                int dimensionCount = 3,
                MorphDimensions::Plane plane = MorphDimensions::Plane::RedBlue) const {
            if (dimensionCount < 3) {
                return sliceReduced(cube, dimension, data, position, MorphDimensions(dimensionCount, plane));
            }
            data.pointOverlaps = false;
            data.lineOverlaps = false;

            int dimX = Vertex::Red;
            int dimY = Vertex::Blue;

            MorphPosition::getOtherDims(dimension, dimX, dimY);
            Vertex2 point(position[dimX], position[dimY]);

            if (!sliceFace(cube.getFace(dimension, VertCube::LowPole), dimX, dimY, point, data.v00, data.v10, data.v0)) {
                return false;
            }

            if (!sliceFace(cube.getFace(dimension, VertCube::HighPole), dimX, dimY, point, data.v01, data.v11, data.v1)) {
                return false;
            }

            data.lineOverlaps = true;
            data.pointOverlaps = containsSlicePosition(data.v0.values[dimension], data.v1.values[dimension], position[dimension]);

            return data.pointOverlaps;
        }

        template<typename GuideApplier, typename ResolveCube>
        const RenderResult& sliceMesh(
                Mesh* mesh,
                const RasterizationRequest& request,
                float oscPhase,
                GuideApplier&& applyGuide,
                RenderResult& output,
                VertCube::ReductionData& reductionData,
                ResolveCube&& resolveCube) const {
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
                        resolveCube(cubes[i]),
                        sliceDimension,
                        independent,
                        oscPhase,
                        request,
                        pointScaling,
                        applyGuide,
                        output,
                        reductionData,
                        request.preparedSlice != nullptr
                                ? &request.preparedSlice->cells[(size_t) i] : nullptr);
            }

            std::sort(output.intercepts.begin(), output.intercepts.end());
            restrict(output.intercepts, request);

            InterceptPaddingFlagPolicy().apply(output.intercepts);
            output.sampleable = output.intercepts.size() >= 2;

            return output;
        }

        template<typename GuideApplier>
        const RenderResult& sliceMesh(
                Mesh* mesh,
                const RasterizationRequest& request,
                float oscPhase,
                GuideApplier&& applyGuide,
                RenderResult& output,
                VertCube::ReductionData& reductionData) const {
            return sliceMesh(
                    mesh,
                    request,
                    oscPhase,
                    std::forward<GuideApplier>(applyGuide),
                    output,
                    reductionData,
                    [](VertCube* cube) { return cube; });
        }

    private:
        bool sliceReduced(
                const VertCube& cube, int axis, VertCube::ReductionData& data,
                const MorphPosition& position, MorphDimensions dimensions) const {
            const int bit = MorphDimensions::cornerBit(axis);
            VecOps::copy(cube.getVertex(0)->values, data.v0.values, Vertex::numElements);
            VecOps::copy(cube.getVertex(bit)->values, data.v1.values, Vertex::numElements);
            if (dimensions.axisCount() == 2 && !dimensions.active(axis)) {
                int first;
                int second;
                MorphPosition::getOtherDims(axis, first, second);
                if (!sliceFace(cube.getFace(axis, VertCube::LowPole),
                        first, second, { position[first], position[second] },
                        data.v00, data.v10, data.v0)) {
                    data.lineOverlaps = false;
                    data.pointOverlaps = false;
                    return false;
                }
                VecOps::copy(data.v0.values, data.v1.values, Vertex::numElements);
                data.v0.values[axis] = 0.f;
                data.v1.values[axis] = 1.f;
            } else if (dimensions.axisCount() == 2) {
                int otherAxis = Vertex::Time;
                for (const int candidate : MorphDimensions::axes) {
                    if (candidate != axis && dimensions.active(candidate)) {
                        otherAxis = candidate;
                    }
                }
                const int otherBit = MorphDimensions::cornerBit(otherAxis);
                if (!containsSlicePosition(cube.getVertex(0)->values[otherAxis],
                            cube.getVertex(otherBit)->values[otherAxis], position[otherAxis])
                        || !containsSlicePosition(cube.getVertex(bit)->values[otherAxis],
                            cube.getVertex(bit | otherBit)->values[otherAxis], position[otherAxis])) {
                    data.lineOverlaps = false;
                    data.pointOverlaps = false;
                    return false;
                }
                VertCube::vertexAt(position[otherAxis], otherAxis,
                        cube.getVertex(0), cube.getVertex(otherBit), &data.v0);
                VertCube::vertexAt(position[otherAxis], otherAxis,
                        cube.getVertex(bit), cube.getVertex(bit | otherBit), &data.v1);
            }
            data.lineOverlaps = true;
            data.pointOverlaps = containsSlicePosition(
                    data.v0.values[axis], data.v1.values[axis], position[axis]);
            return data.pointOverlaps;
        }

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
                GuideApplier&& applyGuide,
                RenderResult& output,
                VertCube::ReductionData& reductionData,
                const VertCube::ReductionData* prepared) const {
            if (prepared != nullptr) {
                PreparedMeshSlice::copy(*prepared, reductionData);
            } else {
                slice(*cube, sliceDimension, reductionData, request.morph, request.dimensionCount, request.twoDimensionalPlane);
            }
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
                    output.colorPoints,
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
                std::vector<ColorPoint>& colorPoints,
                VertCube::ReductionData& reductionData) const {
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
                    colorPoints);
        }

        static void restrict(std::vector<Intercept>& intercepts, const RasterizationRequest& request) {
            InterceptRestrictionPolicy::Context context;
            context.cyclic = request.cyclic;
            context.minimumX = request.xMinimum;
            context.maximumX = request.xMaximum;

            InterceptRestrictionPolicy(context).restrict(intercepts);
        }

        static bool sliceFace(
                const VertCube::Face& face,
                int dimX,
                int dimY,
                const Vertex2& point,
                Vertex& x0,
                Vertex& x1,
                Vertex& output) {
            float minX = jmin(face.v00->values[dimX], face.v11->values[dimX]);
            float minY = jmin(face.v00->values[dimY], face.v11->values[dimY]);
            float maxX = jmax(face.v00->values[dimX], face.v11->values[dimX]);
            float maxY = jmax(face.v00->values[dimY], face.v11->values[dimY]);

            expandUnitUpperBoundary(maxX);
            expandUnitUpperBoundary(maxY);

            if (point.x < minX || point.x >= maxX || point.y < minY || point.y >= maxY) {
                return false;
            }

            VertCube::vertexAt(point.y, dimY, face.v00, face.v01, &x0);
            VertCube::vertexAt(point.y, dimY, face.v10, face.v11, &x1);
            VertCube::vertexAt(point.x, dimX, &x0, &x1, &output);

            return true;
        }

        static void expandUnitUpperBoundary(float& value) {
            if (value == 1.f) {
                value += 0.000001f;
            }
        }

        static bool containsSlicePosition(float a, float b, float position) {
            if (position == 1.f || position == 0.f) {
                return a == position || b == position;
            }

            return NumberUtils::withinExclUpper(position, jmin(a, b), jmax(a, b));
        }
    };
}

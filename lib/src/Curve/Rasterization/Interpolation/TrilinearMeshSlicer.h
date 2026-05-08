#pragma once

#include "../../VertCube.h"
#include "../../Vertex2.h"
#include "../../../Obj/MorphPosition.h"
#include "../../../Util/NumberUtils.h"

namespace Rasterization {
    class TrilinearMeshSlicer {
    public:
        constexpr TrilinearMeshSlicer() = default;

        bool slice(
                const VertCube& cube,
                int dimension,
                VertCube::ReductionData& data,
                const MorphPosition& position) const {
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

    private:
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

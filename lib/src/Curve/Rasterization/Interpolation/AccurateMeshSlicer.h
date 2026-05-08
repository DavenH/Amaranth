#pragma once

#include "../../VertCube.h"
#include "../../Vertex2.h"
#include "../../../Obj/MorphPosition.h"

namespace Rasterization {
    class AccurateMeshSlicer {
    public:
        constexpr AccurateMeshSlicer() = default;

        bool slice(
                const VertCube& cube,
                int dimension,
                VertCube::ReductionData& data,
                const MorphPosition& position) const {
            data.pointOverlaps = true;
            data.lineOverlaps = true;

            int dimX = 0;
            int dimY = 0;

            MorphPosition::getOtherDims(dimension, dimX, dimY);
            Vertex2 point(position[dimX], position[dimY]);
            float primeVal = position[dimension];

            if (! sliceFace(cube.getFace(dimension, VertCube::LowPole), dimX, dimY, point, data.v0)) {
                markMiss(data);
                return false;
            }

            if (! sliceFace(cube.getFace(dimension, VertCube::HighPole), dimX, dimY, point, data.v1)) {
                markMiss(data);
                return false;
            }

            float valA = data.v0.values[dimension];
            float valB = data.v1.values[dimension];

            data.pointOverlaps = (valA > valB) ?
                    valB <= primeVal && primeVal < valA :
                    valA <= primeVal && primeVal < valB;

            return data.pointOverlaps;
        }

    private:
        static bool isMeaningfullyNonZero(float value) {
            return value > 1e-4f || value < -1e-4f;
        }

        static void markMiss(VertCube::ReductionData& data) {
            data.pointOverlaps = false;
            data.lineOverlaps = false;
        }

        static bool sliceFace(
                const VertCube::Face& face,
                int dimX,
                int dimY,
                const Vertex2& point,
                Vertex& output) {
            float xFracA = 1.f;
            float xFracB = 1.f;
            float yFracA = 1.f;
            float yFracB = 1.f;

            Vertex2 nnv(face.v00->values[dimX], face.v00->values[dimY]);
            Vertex2 nxv(face.v01->values[dimX], face.v01->values[dimY]);
            Vertex2 xnv(face.v10->values[dimX], face.v10->values[dimY]);
            Vertex2 xxv(face.v11->values[dimX], face.v11->values[dimY]);

            Vertex2 diffYminX = nxv - nnv;
            Vertex2 diffXminY = xnv - nnv;

            float denom = (nnv - nxv).cross(xnv - xxv);
            if (isMeaningfullyNonZero(denom)) {
                Vertex2 icptY = (nnv - nxv) * ((xxv - nxv).cross(xnv - xxv) / denom) + nxv;

                float icptDenom = (point - icptY).cross(nnv - xnv);
                xFracA = (xnv - icptY).cross(nnv - xnv) / icptDenom;

                icptDenom = (point - icptY).cross(nxv - xxv);
                xFracB = (xxv - icptY).cross(nxv - xxv) / icptDenom;
            } else {
                xFracA = xFracB = (point.x - nnv.x) / diffXminY.x;
            }

            denom = (nxv - xxv).cross(nnv - xnv);
            if (isMeaningfullyNonZero(denom)) {
                Vertex2 icptX = (nxv - xxv) * ((nxv - xxv).cross(nnv - xnv) / denom) + xxv;

                float icptDenom = (point - icptX).cross(nxv - nnv);
                yFracA = (nnv - icptX).cross(nxv - nnv) / icptDenom;

                icptDenom = (point - icptX).cross(xxv - xnv);
                yFracB = (xnv - icptX).cross(xxv - xnv) / icptDenom;
            } else {
                yFracA = yFracB = (point.y - nnv.y) / diffYminX.y;
            }

            if (xFracA >= 1.f || xFracA < 0.f || yFracA >= 1.f || yFracA < 0.f) {
                return false;
            }

            float nnFrac = (1.f - xFracA) * (1.f - yFracA);
            float xnFrac = xFracA        * (1.f - yFracA);
            float nxFrac = (1.f - xFracB) * yFracB;
            float xxFrac = xFracB        * yFracB;

            output = *face.v00 * nnFrac
                     + *face.v10 * xnFrac
                     + *face.v01 * nxFrac
                     + *face.v11 * xxFrac;

            return true;
        }
    };
}

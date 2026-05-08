#pragma once

#include "../../Obj/MorphPosition.h"

class MeshRasterizer;

namespace Rasterization {
    struct MeshRasterizerRenderState {
        bool batchMode {};
        bool lowResCurves {};
        bool calcDepthDims {};
        int scalingType { 1 };
        MorphPosition pos;

        MeshRasterizerRenderState() = default;
        MeshRasterizerRenderState(
                bool batch,
                bool lowres,
                bool calcDepth,
                int scaling,
                const MorphPosition& pos) :
                batchMode(batch)
            ,   lowResCurves(lowres)
            ,   calcDepthDims(calcDepth)
            ,   scalingType(scaling)
            ,   pos(pos) {
        }
    };

    class ScopedMeshRasterizerRenderState {
    public:
        MeshRasterizer* rasterizer {};
        MeshRasterizerRenderState* state {};

        ScopedMeshRasterizerRenderState(MeshRasterizer* rasterizer, MeshRasterizerRenderState* state);
        ~ScopedMeshRasterizerRenderState();
    };
}

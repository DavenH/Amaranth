#pragma once

#include "../../Obj/MorphPosition.h"

namespace Rasterization {
    struct RasterizerRenderState {
        bool batchMode {};
        bool lowResCurves {};
        bool calcDepthDims {};
        int scalingType { 1 };
        MorphPosition pos;

        RasterizerRenderState() = default;

        RasterizerRenderState(
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
}

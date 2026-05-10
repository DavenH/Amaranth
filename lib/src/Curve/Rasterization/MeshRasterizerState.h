#pragma once

#include "RenderState.h"

class MeshRasterizer;

namespace Rasterization {
    using MeshRasterizerRenderState = RasterizerRenderState;

    class ScopedMeshRasterizerRenderState {
    public:
        MeshRasterizer* rasterizer {};
        MeshRasterizerRenderState* state {};

        ScopedMeshRasterizerRenderState(MeshRasterizer* rasterizer, MeshRasterizerRenderState* state);
        ~ScopedMeshRasterizerRenderState();
    };
}

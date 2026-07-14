#pragma once

#include <Curve/Rasterization/Policies/Core/PointScalingPolicy.h>
#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>

namespace Rasterization {

class GraphicRasterizer : public TrilinearMeshRasterizer {
public:
    enum class Scaling {
        Unipolar = 0,
        Bipolar = 1,
        HalfBipolar = 2
    };

    struct RenderState {
        bool batchMode {};
        bool lowResCurves {};
        bool calcDepthDims {};
        bool restoreMesh {};
        int noiseSeed { -1 };
        int scalingType { 1 };
        Mesh* mesh {};
        MorphPosition pos;

        RenderState() = default;
        RenderState(
                bool batch,
                bool lowres,
                bool calcDepth,
                int scaling,
                const MorphPosition& pos);
    };

    class ScopedRenderState {
    public:
        ScopedRenderState(GraphicRasterizer* rasterizer, RenderState* state);
        ~ScopedRenderState();

    private:
        GraphicRasterizer* rasterizer {};
        RenderState* state {};
    };

    GraphicRasterizer(bool cyclic, float margin);

    void restoreStateFrom(RenderState& state);
    void saveStateTo(RenderState& state);
    ScopedRenderState preserveState(RenderState& state) { return ScopedRenderState(this, &state); }

    int getNoiseSeed() const { return getRequest().noiseSeed; }

    void updateGeometry() override;
    void updateGeometry(Mesh* mesh, float oscPhase = 0.f);
    void updateWaveform() override;
    void updateWaveform(Mesh* mesh, float oscPhase = 0.f);
    void cleanUp();
    void reset() { cleanUp(); }

    RenderState createRenderState();

    static RenderState createBatchRenderState(
            Scaling scaling,
            const MorphPosition& morphPosition,
            bool lowResCurves = true,
            bool calcDepthDimensions = false);

    void setBatchMode(bool batch) { getRequest().batchMode = batch; }

private:
    static PointScalingMode scalingModeFromRenderState(int scalingType);
    static int renderStateScalingType(PointScalingMode scalingMode);

    void updateGeometryAtPhase(float oscPhase);
    void updateWaveformAtPhase(float oscPhase);
};

}

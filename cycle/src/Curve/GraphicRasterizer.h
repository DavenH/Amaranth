#pragma once

#include <Curve/Rasterization/Policies/Core/PointScalingPolicy.h>
#include <Curve/Rasterization/TrilinearMeshRasterizer.h>
#include <Obj/Ref.h>

#include "Rasterization/Policies/Graphic/GraphicPolicies.h"
#include "../Updating/DynamicDetailUpdater.h"

class Interactor;

class GraphicRasterizer :
        public DynamicDetailUpdateable
    ,	public virtual SingletonAccessor
    ,   public Rasterization::TrilinearMeshRasterizer {
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
        int scalingType { 1 };
        Mesh* mesh {};
        MorphPosition pos;

        RenderState() = default;

        RenderState(
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

    class ScopedRenderState {
    public:
        ScopedRenderState(GraphicRasterizer* rasterizer, RenderState* state);
        ~ScopedRenderState();

    private:
        GraphicRasterizer* rasterizer {};
        RenderState* state {};
    };

    GraphicRasterizer(SingletonRepo* repo,
                      Interactor* interactor,
                      const String& name, int layerGroup,
                      bool cyclic, float margin);

    void pullModPositionAndAdjust();
    void restoreStateFrom(RenderState& state);
    void saveStateTo(RenderState& state);
    ScopedRenderState preserveState(RenderState& state) { return ScopedRenderState(this, &state); }

    void updateGeometry() override;
    void updateGeometry(Mesh* mesh, float oscPhase = 0.f);
    void updateWaveform() override;
    void updateWaveform(Mesh* mesh, float oscPhase = 0.f);
    void cleanUp();
    void reset() override { cleanUp(); }

    RenderState createRenderState() {
        RenderState state;
        saveStateTo(state);
        return state;
    }

    static RenderState createBatchRenderState(
            Scaling scaling,
                const MorphPosition& morphPosition,
                bool lowResCurves = true,
                bool calcDepthDimensions = false) {
        return RenderState(
                true,
                lowResCurves,
                calcDepthDimensions,
                static_cast<int>(scaling),
                morphPosition);
    }

    Interactor* getInteractor() const { return interactor; }

    void setBatchMode(bool batch) { rasterizer.getRequest().batchMode = batch; }

private:
    static Rasterization::PointScalingMode scalingModeFromRenderState(int scalingType);
    static int renderStateScalingType(Rasterization::PointScalingMode scalingMode);

    int primaryViewDimension();
    void prepareRequestForRender();
    void updateGeometryAtPhase(float oscPhase);
    void updateWaveformAtPhase(float oscPhase);

    int layerGroup;
    Interactor* interactor;
};

typedef GraphicRasterizer TimeRasterizer;
typedef GraphicRasterizer SpectRasterizer;
typedef GraphicRasterizer PhaseRasterizer;

#pragma once

#include <Curve/Rasterization/Policies/Core/PointScalingPolicy.h>
#include <Curve/Rasterization/TrilinearMeshRasterizer.h>
#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>

#include "Rasterization/Policies/Graphic/GraphicPolicies.h"
#include "../Updating/DynamicDetailUpdater.h"

class Interactor;

class GraphicRasterizer :
    public Updateable
    ,	public DynamicDetailUpdateable
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

    void calcCrossPoints(Mesh* mesh, float oscPhase);
    void cleanUp();
    void performUpdate(UpdateType updateType) override;
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
    void setDims(const Dimensions& dims) { rasterizer.getRequest().dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) { rasterizer.setGuideCurveProvider(provider); }
    void setNoiseSeed(int seed) { rasterizer.getRequest().noiseSeed = seed; }
    void updateOffsetSeeds(int layerSize, int tableSize) { rasterizer.updateOffsetSeeds(layerSize, tableSize); }

private:
    static Rasterization::PointScalingMode scalingModeFromRenderState(int scalingType);
    static int renderStateScalingType(Rasterization::PointScalingMode scalingMode);

    int primaryViewDimension();

    int layerGroup;
    Interactor* interactor;
};

typedef GraphicRasterizer TimeRasterizer;
typedef GraphicRasterizer SpectRasterizer;
typedef GraphicRasterizer PhaseRasterizer;

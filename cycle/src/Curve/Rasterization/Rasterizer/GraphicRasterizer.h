#pragma once

#include <App/SingletonAccessor.h>
#include <Curve/Rasterization/Policies/Graphic/GraphicPolicies.h>
#include <Curve/Rasterization/Rasterizer/GraphicMeshRasterizer.h>
#include <Updating/DynamicDetailUpdater.h>

class Interactor;

class GraphicRasterizer :
        public DynamicDetailUpdateable
    ,   public virtual SingletonAccessor
    ,   public Rasterization::GraphicRasterizer {
public:
    GraphicRasterizer(
            SingletonRepo* repo,
            Interactor* interactor,
            const String& name,
            int layerGroup,
            bool cyclic,
            float margin);

    void pullModPositionAndAdjust();

    void updateGeometry() override;
    void updateGeometry(Mesh* mesh, float oscPhase = 0.f);
    void updateWaveform() override;
    void updateWaveform(Mesh* mesh, float oscPhase = 0.f);
    void reset() override { cleanUp(); }

    Interactor* getInteractor() const { return interactor; }

private:
    void prepareRequestForRender();

    int layerGroup;
    Interactor* interactor;
};

using TimeRasterizer = GraphicRasterizer;
using SpectRasterizer = GraphicRasterizer;
using PhaseRasterizer = GraphicRasterizer;

#pragma once

#include <Curve/MeshRasterizer.h>
#include <Curve/V2/Runtime/V2GraphicRasterizer.h>
#include "../Updating/DynamicDetailUpdater.h"

class GraphicRasterizer :
        public MeshRasterizer
    ,	public DynamicDetailUpdateable
    ,	public virtual SingletonAccessor
{
public:
    GraphicRasterizer(SingletonRepo* repo,
                      Interactor* interactor,
                      const String& name, int layerGroup,
                      bool cyclic, float margin);

    void calcCrossPoints() override;
    void pullModPositionAndAdjust() override;
    int getPrimaryViewDimension() override;

    Interactor* getInteractor() const { return interactor; }

private:
    bool renderWithV2();

    int layerGroup;
    Interactor* interactor;
    V2GraphicRasterizer v2GraphicRasterizer;
    ScopedAlloc<float> v2RenderMemory;
};

typedef GraphicRasterizer TimeRasterizer;
typedef GraphicRasterizer SpectRasterizer;
typedef GraphicRasterizer PhaseRasterizer;

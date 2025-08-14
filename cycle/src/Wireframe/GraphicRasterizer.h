#pragma once

#include "../Updating/DynamicDetailUpdater.h"
#include <Wireframe/OldMeshRasterizer.h>

class GraphicRasterizer :
        public OldMeshRasterizer
    ,	public DynamicDetailUpdateable
    ,	public virtual SingletonAccessor
{
public:
    GraphicRasterizer(SingletonRepo* repo,
                      Interactor* interactor,
                      const String& name, int layerGroup,
                      bool cyclic, float margin);

    void pullModPositionAndAdjust() override;
    int getPrimaryViewDimension() override;

    Interactor* getInteractor() const { return interactor; }

private:
    int layerGroup;
    Interactor* interactor;
};

typedef GraphicRasterizer TimeRasterizer;
typedef GraphicRasterizer SpectRasterizer;
typedef GraphicRasterizer PhaseRasterizer;

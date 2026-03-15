#pragma once

#include <Curve/MeshRasterizer.h>
#include <Curve/V2/Rasterizers/V2GraphicRasterizer.h>
#include "../Updating/DynamicDetailUpdater.h"

/*
 * Graphic rasterizers serve editor and display views.
 *
 * Contract:
 * - render a visually coherent cross-section of the current cycle,
 * - allow self-cyclic closure of the current cycle instead of audio-style chaining,
 * - remain finite and deterministic,
 * - do not need to match voice-chain seam behavior at the phase boundary.
 */
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
};

typedef GraphicRasterizer TimeRasterizer;
typedef GraphicRasterizer SpectRasterizer;
typedef GraphicRasterizer PhaseRasterizer;

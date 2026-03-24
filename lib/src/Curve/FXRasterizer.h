#pragma once

#include <App/SingletonAccessor.h>
#include <Curve/V2/Rasterizers/V2FxRasterizer.h>

#include "MeshRasterizer.h"

/*
 * FX rasterizers render general-purpose curves with padding.
 *
 * Contract:
 * - produce finite, sorted, sampleable output,
 * - apply padding without creating invalid geometry,
 * - avoid voice-style chaining requirements unless a caller explicitly adds them.
 */
class FXRasterizer:
        public MeshRasterizer
    ,   public SingletonAccessor {
public:
    explicit FXRasterizer(SingletonRepo* repo, const String& name = String());
    bool hasEnoughCubesForCrossSection() override;
    int  getNumDims() override;
    void calcCrossPoints() override;
    void cleanUp() override;
    void padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) override;
    void setMesh(Mesh* newMesh) override;

private:
    bool renderWithV2();

    V2FxRasterizer v2FxRasterizer;

    JUCE_LEAK_DETECTOR(FXRasterizer)
};

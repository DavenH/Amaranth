#pragma once

#include <App/SingletonAccessor.h>
#include <Curve/V2/Runtime/V2FxRasterizer.h>

#include "MeshRasterizer.h"

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

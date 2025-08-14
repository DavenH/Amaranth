#pragma once

#include <App/SingletonAccessor.h>

#include "../OldMeshRasterizer.h"

class FXRasterizer:
        public OldMeshRasterizer
    ,   public SingletonAccessor {
    JUCE_LEAK_DETECTOR(FXRasterizer)

public:
    explicit FXRasterizer(SingletonRepo* repo, const String& name = String());
    bool hasEnoughCubesForCrossSection() override;
    int  getNumDims() override;
    void generateControlPoints() override;
    void cleanUp() override;
    void padControlPoints(vector<Intercept>& controlPoints, vector<CurvePiece>& curves) override;
    void setMesh(Mesh* newMesh) override;
};

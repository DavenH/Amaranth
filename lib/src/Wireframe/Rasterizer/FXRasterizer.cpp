#include <algorithm>
#include "FXRasterizer.h"

FXRasterizer::FXRasterizer(SingletonRepo* repo, const String& name) :
        SingletonAccessor(repo, name),
        OldMeshRasterizer(name) {
    cyclic = false;
    calcDepthDims = false;

    dims.x = Vertex::Phase;
    dims.y = Vertex::Amp;
}

void FXRasterizer::generateControlPoints() {
    if (mesh == nullptr || mesh->getNumVerts() == 0) {
        cleanUp();
        return;
    }

    controlPoints.clear();
    for(auto vert : mesh->getVerts()) {
        float* values = vert->values;
        Intercept icpt(values[dims.x], values[dims.y], 0, values[Vertex::Curve]);

        if(scalingType) {
            icpt.y = 2.f * icpt.y - 1.f;
        }

        // time and phase are x and y in this context
        controlPoints.push_back(icpt);
    }

    if (controlPoints.empty()) {
        cleanUp();
        return;
    }

    std::sort(controlPoints.begin(), controlPoints.end());

    curves.clear();

    if (controlPoints.size() < 2) {
        cleanUp();
        return;
    }

    padControlPoints(controlPoints, curves);
    updateCurves();

    unsampleable = false;
}

void FXRasterizer::padControlPoints(vector<Intercept>& controlPoints, vector<CurvePiece>& curves) {
    paddingSize = 1;
    int end = controlPoints.size() - 1;

    Intercept front1(-1.0f, controlPoints[0].y);
    Intercept front2(-0.5f, controlPoints[0].y);

    Intercept back1(1.5f, controlPoints[end].y);
    Intercept back2(2.0f, controlPoints[end].y);

    for(auto& curve : curves) {
        curve.destruct();
    }

    curves.clear();
    curves.reserve(controlPoints.size() + 2);
    curves.emplace_back(front1, front2, controlPoints[0]);
    curves.emplace_back(front2, controlPoints[0], controlPoints[1]);

    for(int i = 0; i < (int) controlPoints.size() - 2; ++i) {
        curves.emplace_back(controlPoints[i], controlPoints[i + 1], controlPoints[i + 2]);
    }

    curves.emplace_back(controlPoints[end - 1], controlPoints[end], back1);
    curves.emplace_back(controlPoints[end], back1, back2);

    for(auto& curve : curves) {
        curve.construct();
    }
}

void FXRasterizer::setMesh(Mesh* newMesh) {
    mesh = newMesh;
}

void FXRasterizer::cleanUp() {
    waveX.nullify();
    waveY.nullify();

    curves.clear();
    controlPoints.clear();
    unsampleable = true;
}

int FXRasterizer::getNumDims() {
    return 1;
}

bool FXRasterizer::hasEnoughCubesForCrossSection() {
    return mesh->getNumVerts() > 1;
}

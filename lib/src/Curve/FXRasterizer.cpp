#include <algorithm>
#include "FXRasterizer.h"

FXRasterizer::FXRasterizer(SingletonRepo* repo, const String& name) :
        SingletonAccessor(repo, name),
        MeshRasterizer(name) {
    cyclic = false;
    calcDepthDims = false;

    dims.x = Vertex::Phase;
    dims.y = Vertex::Amp;
}

void FXRasterizer::calcCrossPoints() {
    if (mesh == nullptr || mesh->getNumVerts() == 0) {
        cleanUp();
        return;
    }

    icpts.clear();
    for(auto vert : mesh->getVerts()) {
        float* values = vert->values;
        Intercept icpt(values[dims.x], values[dims.y], 0, values[Vertex::Curve]);

        if(scalingType) {
            icpt.y = 2.f * icpt.y - 1.f;
        }

        // time and phase are x and y in this context
        icpts.push_back(icpt);
    }

    if (icpts.empty()) {
        cleanUp();
        return;
    }

    std::sort(icpts.begin(), icpts.end());

    curves.clear();

    if (icpts.size() < 2) {
        cleanUp();
        return;
    }

    padIcpts(icpts, curves);
    updateCurves();

    unsampleable = false;
}

void FXRasterizer::padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) {
    paddingSize = 1;
    int end = icpts.size() - 1;

    Intercept front1(-1.0f, icpts[0].y);
    Intercept front2(-0.5f, icpts[0].y);

    Intercept back1(1.5f, icpts[end].y);
    Intercept back2(2.0f, icpts[end].y);

    for(auto& curve : curves) {
        curve.destruct();
    }

    curves.clear();
    curves.reserve(icpts.size() + 2);
    curves.emplace_back(front1, front2, icpts[0]);
    curves.emplace_back(front2, icpts[0], icpts[1]);

    for(int i = 0; i < (int) icpts.size() - 2; ++i) {
        curves.emplace_back(icpts[i], icpts[i + 1], icpts[i + 2]);
    }

    curves.emplace_back(icpts[end - 1], icpts[end], back1);
    curves.emplace_back(icpts[end], back1, back2);

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
    icpts.clear();
    unsampleable = true;
}

int FXRasterizer::getNumDims() {
    return 1;
}

bool FXRasterizer::hasEnoughCubesForCrossSection() {
    return mesh->getNumVerts() > 1;
}

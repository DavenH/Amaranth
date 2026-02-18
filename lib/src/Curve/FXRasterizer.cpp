#include <algorithm>

#include "FXRasterizer.h"

namespace {
constexpr int kV2FxMaxIntercepts = 128;
constexpr int kV2FxMaxCurves = 256;
constexpr int kV2FxMaxWavePoints = 4096;
}

namespace {
    String describeFxMesh(Mesh* mesh) {
        if (mesh == nullptr) {
            return "mesh=null";
        }

        return "mesh=" + String::toHexString((pointer_sized_int) mesh)
            + " verts=" + String(mesh->getNumVerts())
            + " cubes=" + String(mesh->getNumCubes());
    }

    String describeFxIntercepts(const vector<Intercept>& icpts) {
        if (icpts.empty()) {
            return "icpts=0";
        }

        float minX = icpts.front().x;
        float maxX = icpts.front().x;
        float minY = icpts.front().y;
        float maxY = icpts.front().y;
        String parts;

        for (int i = 0; i < (int) icpts.size(); ++i) {
            const Intercept& icpt = icpts[i];
            minX = jmin(minX, icpt.x);
            maxX = jmax(maxX, icpt.x);
            minY = jmin(minY, icpt.y);
            maxY = jmax(maxY, icpt.y);

            if (i < 6) {
                if (parts.isNotEmpty()) {
                    parts += ", ";
                }

                parts += "#" + String(i)
                    + "(" + String(icpt.x, 4)
                    + "," + String(icpt.y, 4)
                    + " shp=" + String(icpt.shp, 4)
                    + ")";
            }
        }

        return "icpts=" + String((int) icpts.size())
            + " x=[" + String(minX, 4) + "," + String(maxX, 4) + "]"
            + " y=[" + String(minY, 4) + "," + String(maxY, 4) + "]"
            + " first={" + parts + "}";
    }
}

FXRasterizer::FXRasterizer(SingletonRepo* repo, const String& name) :
        SingletonAccessor(repo, name),
        MeshRasterizer(name) {
    cyclic = false;
    calcDepthDims = false;

    dims.x = Vertex::Phase;
    dims.y = Vertex::Amp;

    V2PrepareSpec prepareSpec;
    prepareSpec.capacities.maxIntercepts = kV2FxMaxIntercepts;
    prepareSpec.capacities.maxCurves = kV2FxMaxCurves;
    prepareSpec.capacities.maxWavePoints = kV2FxMaxWavePoints;
    prepareSpec.capacities.maxDeformRegions = 0;
    v2FxRasterizer.prepare(prepareSpec);
}

void FXRasterizer::calcCrossPoints() {
    if (renderWithV2()) {
        return;
    }

    if (mesh == nullptr || mesh->getNumVerts() == 0) {
        DBG(MeshRasterizer::getName() + "::calcCrossPoints cleanup empty " + describeFxMesh(mesh));
        cleanUp();
        return;
    }

    DBG(MeshRasterizer::getName() + "::calcCrossPoints begin " + describeFxMesh(mesh));

    icpts.clear();
    for(auto vert : mesh->getVerts()) {
        float* values = vert->values;
        Intercept icpt(values[dims.x], values[dims.y], 0, values[Vertex::Curve]);

        if(scalingType) {
            icpt.y = 2.f * icpt.y - 1.f;
        }

        icpt.adjustedX = icpt.x;

        // time and phase are x and y in this context
        icpts.push_back(icpt);
    }

    if (icpts.empty()) {
        cleanUp();
        return;
    }

    std::sort(icpts.begin(), icpts.end());
    restrictIntercepts(icpts);

    // DBG(MeshRasterizer::getName() + "::calcCrossPoints intercepts " + describeFxIntercepts(icpts));

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
    DBG(MeshRasterizer::getName() + "::setMesh " + describeFxMesh(newMesh));
    mesh = newMesh;
}

void FXRasterizer::cleanUp() {
    waveX.nullify();
    waveY.nullify();

    curves.clear();
    icpts.clear();
    unsampleable = true;

    DBG(MeshRasterizer::getName() + "::cleanUp");
}

int FXRasterizer::getNumDims() {
    return 1;
}

bool FXRasterizer::hasEnoughCubesForCrossSection() {
    return mesh->getNumVerts() > 1;
}

bool FXRasterizer::renderWithV2() {
    if (mesh == nullptr || ! hasEnoughCubesForCrossSection()) {
        return false;
    }

    v2FxRasterizer.setMeshSnapshot(mesh);

    V2FxControlSnapshot controls;
    controls.morph = morph;
    controls.scaling = scalingType;
    controls.wrapPhases = cyclic;
    controls.cyclic = cyclic;
    controls.minX = xMinimum;
    controls.maxX = xMaximum;
    controls.interpolateCurves = interpolateCurves;
    controls.lowResolution = lowResCurves;
    controls.integralSampling = integralSampling;
    v2FxRasterizer.updateControlData(controls);

    std::vector<Intercept> intercepts;
    int interceptCount = 0;
    if (! v2FxRasterizer.extractIntercepts(intercepts, interceptCount) || interceptCount <= 1) {
        return false;
    }

    icpts.assign(intercepts.begin(), intercepts.begin() + interceptCount);
    std::sort(icpts.begin(), icpts.end());

    curves.clear();
    padIcpts(icpts, curves);
    updateCurves();
    unsampleable = false;
    return true;
}

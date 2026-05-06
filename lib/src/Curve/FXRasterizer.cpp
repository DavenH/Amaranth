#include <algorithm>
#include "FXRasterizer.h"
#include "Rasterization/Policies/PaddingPolicy.h"
#include "Rasterization/Policies/PointScalingPolicy.h"

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
}

void FXRasterizer::calcCrossPoints() {
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

        icpt.y = Rasterization::PointScalingPolicy::fromLegacyFxScalingType(scalingType)
                .scale(icpt.y);

        icpt.adjustedX = icpt.x;

        // time and phase are x and y in this context
        icpts.push_back(icpt);
    }

    if (icpts.empty()) {
        // DBG(MeshRasterizer::getName() + "::calcCrossPoints cleanup no-intercepts " + describeFxMesh(mesh));
        cleanUp();
        return;
    }

    std::sort(icpts.begin(), icpts.end());
    restrictIntercepts(icpts);

    // DBG(MeshRasterizer::getName() + "::calcCrossPoints intercepts " + describeFxIntercepts(icpts));

    curves.clear();

    if (icpts.size() < 2) {
        // DBG(MeshRasterizer::getName() + "::calcCrossPoints cleanup too-few-intercepts count=" + String((int) icpts.size())
        //     + " " + describeFxMesh(mesh));
        cleanUp();
        return;
    }

    padIcpts(icpts, curves);
    updateCurves();

    unsampleable = false;
    // DBG(MeshRasterizer::getName() + "::calcCrossPoints ready icpts=" + String((int) icpts.size())
    //     + " curves=" + String((int) curves.size())
    //     + " waveX=" + String(waveX.size())
    //     + " waveY=" + String(waveY.size())
    //     + " " + describeFxMesh(mesh));
}

void FXRasterizer::padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) {
    paddingSize = Rasterization::FxPaddingPolicy().build(icpts, curves);
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

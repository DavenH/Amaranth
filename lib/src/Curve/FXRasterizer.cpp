#include "FXRasterizer.h"

namespace {
    String describeFxMesh(Mesh* mesh) {
        if (mesh == nullptr) {
            return "mesh=null";
        }

        return "mesh=" + String::toHexString((pointer_sized_int) mesh)
            + " verts=" + String(mesh->getNumVerts())
            + " cubes=" + String(mesh->getNumCubes());
    }

    String describeFxSource(Mesh* mesh, int sourceSize) {
        return describeFxMesh(mesh) + " sourceVerts=" + String(sourceSize);
    }
}

FXRasterizer::FXRasterizer(SingletonRepo* repo, const String& name) :
        MeshRasterizer(name)
    ,   SingletonAccessor(repo, name) {
    setWrapsEnds(false);
    setCalcDepthDimensions(false);

    Dimensions fxDimensions(Vertex::Phase, Vertex::Amp);
    setDims(fxDimensions);
    setNumDimensionsProvider([]() { return 1; });
    setCrossSectionAvailabilityProvider([this]() {
        return adapter.hasEnoughCubesForCrossSection();
    });
    setCleanupProvider([this](Rasterization::RasterizerRuntime runtime) {
        adapter.clean(runtime);

        DBG(MeshRasterizer::getName() + "::cleanUp");
    });
}

void FXRasterizer::calcCrossPoints() {
    DBG(MeshRasterizer::getName() + "::calcCrossPoints begin " + describeFxSource(getMesh(), adapter.sourceSize()));
    adapter.render(createFxRequest(), createRasterizerRuntime());
    // DBG(MeshRasterizer::getName() + "::calcCrossPoints ready icpts=" + String((int) icpts.size())
    //     + " curves=" + String((int) curves.size())
    //     + " waveX=" + String(waveX.size())
    //     + " waveY=" + String(waveY.size())
    //     + " " + describeFxMesh(mesh));
}

Rasterization::RasterizationRequest FXRasterizer::createFxRequest() {
    Rasterization::RasterizationRequest request = createRasterizationRequest();
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = Rasterization::pointScalingModeFromLegacyFx(getScalingType());

    return request;
}

void FXRasterizer::padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) {
    adapter.buildPadding(icpts, curves, paddingSize);
}

void FXRasterizer::setMesh(Mesh* newMesh) {
    DBG(MeshRasterizer::getName() + "::setMesh " + describeFxMesh(newMesh));
    MeshRasterizer::setMesh(newMesh);
    adapter.setMesh(newMesh);
}

void FXRasterizer::setVertices(vector<Vertex*>* vertices) {
    DBG(MeshRasterizer::getName() + "::setVertices verts=" + String(vertices == nullptr ? 0 : (int) vertices->size()));
    MeshRasterizer::setMesh(nullptr);
    adapter.setVertices(vertices);
}

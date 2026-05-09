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
    setPaddingProvider([this](vector<Intercept>& intercepts, vector<Curve>& curves) {
        adapter.buildPadding(intercepts, curves, paddingSize);
    });
    setMeshAssignmentProvider([this](Mesh* newMesh) {
        DBG(MeshRasterizer::getName() + "::setMesh " + describeFxMesh(newMesh));
        adapter.setMesh(newMesh);
    });
    setCrossPointProvider([this]() {
        DBG(MeshRasterizer::getName() + "::calcCrossPoints begin "
            + describeFxSource(getMesh(), adapter.sourceSize()));
        adapter.render(createFxRequest(), createRasterizerRuntime());
    });
}

Rasterization::RasterizationRequest FXRasterizer::createFxRequest() {
    Rasterization::RasterizationRequest request = createRasterizationRequest();
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = Rasterization::pointScalingModeFromLegacyFx(getScalingType());

    return request;
}

void FXRasterizer::setVertices(vector<Vertex*>* vertices) {
    DBG(MeshRasterizer::getName() + "::setVertices verts=" + String(vertices == nullptr ? 0 : (int) vertices->size()));
    MeshRasterizer::setMesh(nullptr);
    adapter.setVertices(vertices);
}

bool FXRasterizer::canRasterizeWaveform() const {
    return adapter.hasEnoughCubesForCrossSection();
}

bool FXRasterizer::isBipolar() const {
    return MeshRasterizer::isBipolar();
}

void FXRasterizer::updateWaveform(UpdateType updateType) {
    performUpdate(updateType);
}

double FXRasterizer::sampleWithInterval(Buffer<float> buffer, double delta, double phase) {
    return MeshRasterizer::sampleWithInterval(buffer, delta, phase);
}

float FXRasterizer::samplePerfectly(double delta, Buffer<float> buffer, double phase) {
    return MeshRasterizer::samplePerfectly(delta, buffer, phase);
}

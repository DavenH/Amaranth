#include "FXRasterizer.h"
#include "Rasterization/Policies/PaddingPolicy.h"
#include "Rasterization/Policies/RasterizerOutputPolicy.h"

namespace {
    String describeFxMesh(Mesh* mesh) {
        if (mesh == nullptr) {
            return "mesh=null";
        }

        return "mesh=" + String::toHexString((pointer_sized_int) mesh)
            + " verts=" + String(mesh->getNumVerts())
            + " cubes=" + String(mesh->getNumCubes());
    }

    String describeFxSource(Mesh* mesh, const Rasterization::VertexListSource& source) {
        return describeFxMesh(mesh) + " sourceVerts=" + String(source.size());
    }
}

FXRasterizer::FXRasterizer(SingletonRepo* repo, const String& name) :
        MeshRasterizer(name)
    ,   SingletonAccessor(repo, name) {
    setWrapsEnds(false);
    setCalcDepthDimensions(false);

    Dimensions fxDimensions(Vertex::Phase, Vertex::Amp);
    setDims(fxDimensions);
    vertexSource.setDimensions(fxDimensions.x, fxDimensions.y);
}

void FXRasterizer::calcCrossPoints() {
    if (vertexSource.empty()) {
        DBG(MeshRasterizer::getName() + "::calcCrossPoints cleanup empty " + describeFxSource(getMesh(), vertexSource));
        cleanUp();
        return;
    }

    DBG(MeshRasterizer::getName() + "::calcCrossPoints begin " + describeFxSource(getMesh(), vertexSource));

    composedRasterizer.reset(new Rasterization::ComposedFxRasterizer(
            Rasterization::RasterizerComposer::fx()
                    .withSource(vertexSource)
                    .withRequest(createFxRequest())
                    .build()));

    const auto& output = composedRasterizer->render();
    if (!output.sampleable) {
        cleanUp();
        return;
    }

    publishPipelineOutput(output);
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
    paddingSize = Rasterization::FxPaddingPolicy().build(icpts, curves);
}

void FXRasterizer::publishPipelineOutput(const Rasterization::FxRasterizationPipeline::Output& output) {
    Rasterization::FxOutputPolicy(Rasterization::WaveformPublication::Assign)
            .publish(output, createRasterizerRuntime());
}

void FXRasterizer::setMesh(Mesh* newMesh) {
    DBG(MeshRasterizer::getName() + "::setMesh " + describeFxMesh(newMesh));
    MeshRasterizer::setMesh(newMesh);
    vertexSource.setVertices(newMesh == nullptr ? nullptr : &newMesh->getVerts());
}

void FXRasterizer::setVertices(vector<Vertex*>* vertices) {
    DBG(MeshRasterizer::getName() + "::setVertices verts=" + String(vertices == nullptr ? 0 : (int) vertices->size()));
    MeshRasterizer::setMesh(nullptr);
    vertexSource.setVertices(vertices);
}

void FXRasterizer::cleanUp() {
    curves.clear();
    icpts.clear();
    markWaveformUnsampleable();
    composedRasterizer.reset();

    DBG(MeshRasterizer::getName() + "::cleanUp");
}

int FXRasterizer::getNumDims() {
    return 1;
}

bool FXRasterizer::hasEnoughCubesForCrossSection() {
    return vertexSource.size() > 1;
}

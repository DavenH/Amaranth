#include <algorithm>
#include "FXRasterizer.h"
#include "Rasterization/Policies/PaddingPolicy.h"

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
    cyclic = false;
    calcDepthDims = false;

    dims.x = Vertex::Phase;
    dims.y = Vertex::Amp;
    vertexSource.setDimensions(dims.x, dims.y);
}

void FXRasterizer::calcCrossPoints() {
    if (vertexSource.empty()) {
        DBG(MeshRasterizer::getName() + "::calcCrossPoints cleanup empty " + describeFxSource(mesh, vertexSource));
        cleanUp();
        return;
    }

    DBG(MeshRasterizer::getName() + "::calcCrossPoints begin " + describeFxSource(mesh, vertexSource));

    Rasterization::RasterizationRequest request = createRasterizationRequest();
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = Rasterization::pointScalingModeFromLegacyFx(scalingType);

    const auto& output = pipeline.render(vertexSource, request);
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

void FXRasterizer::padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) {
    paddingSize = Rasterization::FxPaddingPolicy().build(icpts, curves);
}

void FXRasterizer::publishPipelineOutput(const Rasterization::FxRasterizationPipeline::Output& output) {
    icpts = output.intercepts;
    curves = output.curves;

    Rasterization::WaveformBufferRefs(
            waveX,
            waveY,
            diffX,
            slope,
            area,
            zeroIndex,
            oneIndex).assignFrom(output.waveform);

    paddingSize = output.paddingSize;
    unsampleable = !output.sampleable;
}

void FXRasterizer::setMesh(Mesh* newMesh) {
    DBG(MeshRasterizer::getName() + "::setMesh " + describeFxMesh(newMesh));
    mesh = newMesh;
    vertexSource.setVertices(newMesh == nullptr ? nullptr : &newMesh->getVerts());
}

void FXRasterizer::setVertices(vector<Vertex*>* vertices) {
    DBG(MeshRasterizer::getName() + "::setVertices verts=" + String(vertices == nullptr ? 0 : (int) vertices->size()));
    mesh = nullptr;
    vertexSource.setVertices(vertices);
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
    return vertexSource.size() > 1;
}

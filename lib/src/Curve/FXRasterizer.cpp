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
        SingletonAccessor(repo, name) {
    rasterizerData.paddingSize = result().paddingSize;
    rasterizerData.wrapsVertices = false;
}

void FXRasterizer::cleanUp() {
    pointListRasterizer.cleanUp();
    publishSnapshot();

    DBG(getName() + "::cleanUp");
}

void FXRasterizer::updateGeometry() {
    DBG(getName() + "::updateGeometry begin " + describeFxSource(mesh, vertexCount()));

    updateFxGeometry(createFxRequest());
    publishSnapshot();
}

void FXRasterizer::updateWaveform() {
    DBG(getName() + "::updateWaveform begin " + describeFxSource(mesh, vertexCount()));

    auto request = createFxRequest();
    vector<Intercept> intercepts;
    copyVertexInterceptsTo(intercepts);
    pointListRasterizer.renderWaveform(
            intercepts,
            request,
            Rasterization::PointListPaddingMode::Fx);

    publishSnapshot();
}

void FXRasterizer::reset() {
    cleanUp();
}

void FXRasterizer::setMesh(Mesh* newMesh) {
    mesh = newMesh;
    DBG(getName() + "::setMesh " + describeFxMesh(newMesh));
    vertices = newMesh == nullptr ? nullptr : &newMesh->getVerts();
}

void FXRasterizer::setVertices(vector<Vertex*>* vertices) {
    DBG(getName() + "::setVertices verts=" + String(vertices == nullptr ? 0 : (int) vertices->size()));
    mesh = nullptr;
    this->vertices = vertices;
}

bool FXRasterizer::canRasterizeWaveform() const {
    return vertexCount() > 1;
}

bool FXRasterizer::isBipolar() const {
    return scalingType == Bipolar || scalingType == HalfBipolar;
}

Rasterization::RasterizationRequest FXRasterizer::createFxRequest() const {
    Rasterization::RasterizationRequest request;
    request.dims = dims;
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = Rasterization::pointScalingModeFromLegacyFx(scalingType);

    return request;
}

Intercept FXRasterizer::interceptAt(Vertex* vertex) const {
    jassert(vertex != nullptr);

    float* values = vertex->values;

    Intercept intercept(
            values[Vertex::Phase],
            values[Vertex::Amp],
            nullptr,
            values[Vertex::Curve]);
    intercept.adjustedX = intercept.x;

    return intercept;
}

bool FXRasterizer::updateFxGeometry(const Rasterization::RasterizationRequest& request) {
    vector<Intercept> intercepts;
    copyVertexInterceptsTo(intercepts);
    pointListRasterizer.renderGeometry(
            intercepts,
            request,
            Rasterization::PointListPaddingMode::Fx);

    return result().curves.size() >= 2;
}

void FXRasterizer::copyVertexInterceptsTo(vector<Intercept>& intercepts) const {
    intercepts.clear();

    if (vertices == nullptr) {
        return;
    }

    intercepts.reserve(vertices->size());

    for (Vertex* vertex : *vertices) {
        if (vertex != nullptr) {
            intercepts.emplace_back(interceptAt(vertex));
        }
    }
}

int FXRasterizer::vertexCount() const {
    return vertices == nullptr ? 0 : (int) vertices->size();
}

void FXRasterizer::publishSnapshot() {
    const auto& renderResult = result();

    Rasterization::RasterizerSnapshotSource snapshot;
    snapshot.intercepts = &renderResult.intercepts;
    snapshot.colorPoints = &renderResult.colorPoints;
    snapshot.curves = &renderResult.curves;
    snapshot.waveform = renderResult.waveform;
    snapshot.paddingSize = renderResult.paddingSize;
    snapshot.wrapsVertices = false;

    Rasterization::BaseRasterizer::publishSnapshot(snapshot);
}

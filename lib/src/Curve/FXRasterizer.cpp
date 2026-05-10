#include "FXRasterizer.h"

#include <algorithm>

#include "Rasterization/Builders/RasterizerSnapshotBuilder.h"
#include "Rasterization/GuideCurveOffsetSeeds.h"
#include "Rasterization/Builders/CurveWaveformBuilder.h"
#include "Rasterization/Policies/Core/InterceptPolicies.h"
#include "Rasterization/Policies/Core/PaddingPolicy.h"
#include "Rasterization/Policies/Core/PointScalingPolicy.h"
#include "Rasterization/Sampling/WaveformSampler.h"

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
}

void FXRasterizer::calcCrossPoints() {
    DBG(getName() + "::calcCrossPoints begin " + describeFxSource(mesh, vertexCount()));

    renderFx(createFxRequest());
    publishSnapshot();
}

void FXRasterizer::cleanUp() {
    result.clear();
    publishSnapshot();

    DBG(getName() + "::cleanUp");
}

void FXRasterizer::performUpdate(UpdateType updateType) {
    if (updateType == Update) {
        calcCrossPoints();
    }
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

bool FXRasterizer::hasEnoughCubesForCrossSection() {
    return vertexCount() > 1;
}

bool FXRasterizer::isBipolar() const {
    return scalingType == Bipolar || scalingType == HalfBipolar;
}

void FXRasterizer::updateWaveform(UpdateType updateType) {
    performUpdate(updateType);
}

void FXRasterizer::sampleEvenlyTo(const Buffer<float>& dest) {
    if (dest.empty()) {
        return;
    }

    samplerView().sampleWithInterval(dest, 1.f / float(dest.size() - 1), 0.f);
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

bool FXRasterizer::renderFx(const Rasterization::RasterizationRequest& request) {
    result.clear();

    if (vertexCount() == 0) {
        return false;
    }

    copyVertexInterceptsTo(result.intercepts);

    Rasterization::PointScalingPolicy pointScaling(request.scalingMode);
    for (auto& intercept : result.intercepts) {
        intercept.y = pointScaling.scale(intercept.y);
    }

    if (result.intercepts.empty()) {
        return false;
    }

    std::sort(result.intercepts.begin(), result.intercepts.end());

    Rasterization::InterceptRestrictionPolicy::Context context;
    context.cyclic = false;
    context.minimumX = request.xMinimum;
    context.maximumX = request.xMaximum;
    Rasterization::InterceptRestrictionPolicy(context).restrict(result.intercepts);

    if (result.intercepts.size() < 2) {
        return false;
    }

    result.paddingSize = Rasterization::FxPaddingPolicy().build(result.intercepts, result.curves);
    bakeWaveform(request);

    return result.sampleable;
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

void FXRasterizer::bakeWaveform(const Rasterization::RasterizationRequest& request) {
    Rasterization::CurveWaveformBuilder::Context context;
    context.request = &request;
    context.offsetSeeds = &guideCurveOffsetSeeds;
    context.paddingSize = result.paddingSize;

    result.sampleable = curveWaveformBuilder.render(
            result.curves,
            context,
            [this](int totalRes) {
                result.waveform.place(result.waveformMemory, totalRes);
                return Rasterization::WaveformBufferRefs(result.waveform);
            });
}

int FXRasterizer::vertexCount() const {
    return vertices == nullptr ? 0 : (int) vertices->size();
}

void FXRasterizer::publishSnapshot() {
    Rasterization::RasterizerSnapshotSource snapshot;
    snapshot.intercepts = &result.intercepts;
    snapshot.colorPoints = &result.colorPoints;
    snapshot.curves = &result.curves;
    snapshot.waveform = result.waveform;

    Rasterization::RasterizerSnapshotBuilder().publish(rasterizerData, snapshot);
}

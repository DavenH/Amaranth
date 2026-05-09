#include "FXRasterizer.h"

#include <algorithm>

#include "Rasterization/Builders/RasterizerSnapshotBuilder.h"
#include "Rasterization/GuideCurveOffsetSeeds.h"
#include "Rasterization/Pipelines/CurveWaveformPipeline.h"
#include "Rasterization/Policies/Core/InterceptRestrictionPolicy.h"
#include "Rasterization/Policies/Core/PaddingPolicy.h"
#include "Rasterization/Policies/Core/PointScalingPolicy.h"
#include "Rasterization/Policies/Core/SnapshotPolicy.h"
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
    DBG(getName() + "::calcCrossPoints begin " + describeFxSource(mesh, source.size()));

    renderFx(createFxRequest());
    publishSnapshot();
}

void FXRasterizer::cleanUp() {
    result.clear();
    publishSnapshot();

    DBG(getName() + "::cleanUp");
}

Rasterization::RasterizationRequest FXRasterizer::createRasterizationRequest() const {
    return createFxRequest();
}

void FXRasterizer::makeCopy() {
    publishSnapshot();
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
    source.setVertices(newMesh == nullptr ? nullptr : &newMesh->getVerts());
}

void FXRasterizer::setVertices(vector<Vertex*>* vertices) {
    DBG(getName() + "::setVertices verts=" + String(vertices == nullptr ? 0 : (int) vertices->size()));
    mesh = nullptr;
    source.setVertices(vertices);
}

bool FXRasterizer::canRasterizeWaveform() const {
    return source.size() > 1;
}

bool FXRasterizer::hasEnoughCubesForCrossSection() {
    return source.size() > 1;
}

bool FXRasterizer::isBipolar() const {
    return scalingType == Bipolar || scalingType == HalfBipolar;
}

bool FXRasterizer::isSampleable() {
    return result.sampler().isSampleable();
}

bool FXRasterizer::isSampleableAt(float x) {
    return result.sampler().isSampleableAt(x);
}

void FXRasterizer::updateWaveform(UpdateType updateType) {
    performUpdate(updateType);
}

float FXRasterizer::sampleAt(double angle) {
    return result.sampler().sampleAt(angle);
}

float FXRasterizer::sampleAt(double angle, int& currentIndex) {
    return result.sampler().sampleAt(angle, currentIndex);
}

double FXRasterizer::sampleWithInterval(Buffer<float> buffer, double delta, double phase) {
    return result.sampler().sampleWithInterval(buffer, delta, phase);
}

float FXRasterizer::samplePerfectly(double delta, Buffer<float> buffer, double phase) {
    return result.sampler().samplePerfectly(delta, buffer, phase);
}

void FXRasterizer::sampleEvenlyTo(const Buffer<float>& dest) {
    if (dest.empty()) {
        return;
    }

    sampleWithInterval(dest, 1.f / float(dest.size() - 1), 0.0);
}

void FXRasterizer::padIcpts(vector<Intercept>& intercepts, vector<Curve>& curves) {
    result.paddingSize = Rasterization::FxPaddingPolicy().build(intercepts, curves);
}

Rasterization::RasterizationRequest FXRasterizer::createFxRequest() const {
    Rasterization::RasterizationRequest request;
    request.dims = dims;
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = Rasterization::pointScalingModeFromLegacyFx(scalingType);

    return request;
}

bool FXRasterizer::renderFx(const Rasterization::RasterizationRequest& request) {
    result.clear();

    if (source.empty()) {
        return false;
    }

    source.copyInterceptsTo(result.intercepts);

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

void FXRasterizer::bakeWaveform(const Rasterization::RasterizationRequest& request) {
    Rasterization::CurveWaveformPipeline::Context context;
    context.request = &request;
    context.offsetSeeds = &guideCurveOffsetSeeds;
    context.paddingSize = result.paddingSize;

    result.sampleable = curveWaveformPipeline.render(
            result.curves,
            context,
            [this](int totalRes) {
                result.waveform.place(result.waveformMemory, totalRes);
                return Rasterization::WaveformBufferRefs(result.waveform);
            });
}

void FXRasterizer::publishSnapshot() {
    Rasterization::RasterizerSnapshotSource snapshot;
    snapshot.intercepts = &result.intercepts;
    snapshot.colorPoints = &result.colorPoints;
    snapshot.curves = &result.curves;
    snapshot.waveform = result.waveform;

    Rasterization::RasterizerSnapshotBuilder().publish(rasterizerData, snapshot);
}

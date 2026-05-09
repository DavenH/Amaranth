#include "FXRasterizer.h"

#include "Rasterization/Builders/RasterizerSnapshotBuilder.h"
#include "Rasterization/Policies/PointScalingPolicy.h"
#include "Rasterization/Policies/SnapshotPolicy.h"
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
    DBG(getName() + "::calcCrossPoints begin " + describeFxSource(mesh, adapter.sourceSize()));

    if (!adapter.render(createFxRequest(), createRasterizerRuntime())) {
        publishSnapshot();
        return;
    }

    publishSnapshot();
}

void FXRasterizer::cleanUp() {
    adapter.clean(createRasterizerRuntime());
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
    adapter.setMesh(newMesh);
}

void FXRasterizer::setVertices(vector<Vertex*>* vertices) {
    DBG(getName() + "::setVertices verts=" + String(vertices == nullptr ? 0 : (int) vertices->size()));
    mesh = nullptr;
    adapter.setVertices(vertices);
}

bool FXRasterizer::canRasterizeWaveform() const {
    return adapter.hasEnoughCubesForCrossSection();
}

bool FXRasterizer::hasEnoughCubesForCrossSection() {
    return adapter.hasEnoughCubesForCrossSection();
}

bool FXRasterizer::isBipolar() const {
    return scalingType == Bipolar || scalingType == HalfBipolar;
}

bool FXRasterizer::isSampleable() {
    return !unsampleable && Rasterization::WaveformSampler::isSampleable(storage.waveform.waveform);
}

bool FXRasterizer::isSampleableAt(float x) {
    return !unsampleable && Rasterization::WaveformSampler::isSampleableAt(storage.waveform.waveform, x);
}

void FXRasterizer::updateWaveform(UpdateType updateType) {
    performUpdate(updateType);
}

float FXRasterizer::sampleAt(double angle) {
    return Rasterization::WaveformSampler::sampleAt(storage.waveform.waveform, unsampleable, angle);
}

float FXRasterizer::sampleAt(double angle, int& currentIndex) {
    return Rasterization::WaveformSampler::sampleAt(storage.waveform.waveform, unsampleable, angle, currentIndex);
}

double FXRasterizer::sampleWithInterval(Buffer<float> buffer, double delta, double phase) {
    return Rasterization::WaveformSampler::sampleWithInterval(
            storage.waveform.waveform,
            buffer,
            delta,
            phase);
}

float FXRasterizer::samplePerfectly(double delta, Buffer<float> buffer, double phase) {
    return Rasterization::WaveformSampler::samplePerfectly(
            storage.waveform.waveform,
            delta,
            buffer,
            phase);
}

void FXRasterizer::sampleEvenlyTo(const Buffer<float>& dest) {
    if (dest.empty()) {
        return;
    }

    sampleWithInterval(dest, 1.f / float(dest.size() - 1), 0.0);
}

void FXRasterizer::padIcpts(vector<Intercept>& intercepts, vector<Curve>& curves) {
    adapter.buildPadding(intercepts, curves, paddingSize);
}

Rasterization::RasterizationRequest FXRasterizer::createFxRequest() const {
    Rasterization::RasterizationRequest request;
    request.dims = dims;
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = Rasterization::pointScalingModeFromLegacyFx(scalingType);

    return request;
}

Rasterization::RasterizerRuntime FXRasterizer::createRasterizerRuntime() {
    Rasterization::RasterizerRuntime runtime;
    runtime.intercepts      = &storage.intercepts.intercepts;
    runtime.curves          = &storage.curves.curves;
    runtime.frontPadding    = &storage.intercepts.frontPadding;
    runtime.backPadding     = &storage.intercepts.backPadding;
    runtime.colorPoints     = &storage.intercepts.colorPoints;
    runtime.waveform        = Rasterization::WaveformBufferRefs(storage.waveform.waveform);
    runtime.paddingSize     = &paddingSize;
    runtime.unsampleable    = &unsampleable;
    runtime.needsResorting  = &needsResorting;

    return runtime;
}

void FXRasterizer::publishSnapshot() {
    Rasterization::RasterizerSnapshotSource source =
            Rasterization::createRasterizerSnapshotSource(createRasterizerRuntime());
    Rasterization::RasterizerSnapshotBuilder().publish(storage.snapshot.rasterizerData, source);
}

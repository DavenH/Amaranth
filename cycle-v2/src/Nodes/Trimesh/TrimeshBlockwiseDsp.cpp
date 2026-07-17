#include "TrimeshBlockwiseDsp.h"

#include <Curve/Curve.h>
#include <Curve/Mesh/Vertex.h>

namespace CycleV2 {

namespace {

void ensureCurveTable() {
    if (Curve::table == nullptr) {
        Curve::calcTable();
    }
}

}

void TrimeshBlockwiseDsp::prepare(
        Mesh* meshToRender,
        const MorphPosition& morphPosition,
        int axis,
        bool shouldWrap) {
    setMesh(meshToRender);
    setMorphPosition(morphPosition);
    setPrimaryViewAxis(axis);
    setCyclic(shouldWrap);
    ensureCurveTable();
    if (mesh != nullptr && mesh->hasEnoughCubesForCrossSection()) {
        rasterizer.renderWaveform({ *mesh, createRequest(), 0.f });
    }
}

void TrimeshBlockwiseDsp::setMesh(Mesh* meshToRender) {
    mesh = meshToRender;
}

void TrimeshBlockwiseDsp::setMorphPosition(const MorphPosition& morphPosition) {
    morph = morphPosition;
}

void TrimeshBlockwiseDsp::setPrimaryViewAxis(int axis) {
    primaryViewAxis = axis;
}

void TrimeshBlockwiseDsp::setCyclic(bool shouldWrap) {
    cyclic = shouldWrap;
}

void TrimeshBlockwiseDsp::renderCycle(
        size_t frameCount,
        PortDomain domain,
        ChannelLayout channelLayout,
        SignalPayload& output) {
    prepare(mesh, morph, primaryViewAxis, cyclic);
    renderPrepared(frameCount, domain, channelLayout, output);
}

void TrimeshBlockwiseDsp::renderPrepared(
        size_t frameCount,
        PortDomain domain,
        ChannelLayout channelLayout,
        SignalPayload& output) {
    output.block.samples.resize(frameCount);
    output.domain = domain;
    output.channelLayout = channelLayout;

    if (frameCount == 0) {
        return;
    }

    outputBuffer(output).zero();

    if (mesh == nullptr || !mesh->hasEnoughCubesForCrossSection()) {
        return;
    }

    sampleOutput(outputBuffer(output));
}

void TrimeshBlockwiseDsp::renderCycleInto(Buffer<float> output) {
    prepare(mesh, morph, primaryViewAxis, cyclic);
    renderPreparedInto(output);
}

void TrimeshBlockwiseDsp::renderPreparedInto(Buffer<float> output) {
    output.zero();
    if (output.empty() || mesh == nullptr || !mesh->hasEnoughCubesForCrossSection()) {
        return;
    }

    sampleOutput(output);
}

Rasterization::RasterizationRequest TrimeshBlockwiseDsp::createRequest() const {
    Rasterization::RasterizationRequest request;
    request.cyclic = cyclic;
    request.xMinimum = cyclic ? -0.05f : 0.f;
    request.xMaximum = cyclic ? 1.05f : 1.f;
    request.morph = morph;
    request.primaryViewDimension = primaryViewAxis;
    request.scalingMode = Rasterization::PointScalingMode::Bipolar;
    request.calcDepthDimensions = false;
    request.lowResCurves = false;
    return request;
}

void TrimeshBlockwiseDsp::sampleOutput(Buffer<float> dest) {
    auto sampler = rasterizer.sampler();

    if (!sampler.isSampleable()) {
        return;
    }

    const float delta = dest.size() > 0 ? 1.f / (float) dest.size() : 0.f;
    int currentIndex = sampler.initialIndex();

    for (int i = 0; i < dest.size(); ++i) {
        const float phase = (float) i * delta;

        if (sampler.isSampleableAt(phase)) {
            dest[i] = sampler.sampleAt(phase, currentIndex);
        }
    }
}

Buffer<float> TrimeshBlockwiseDsp::outputBuffer(SignalPayload& output) const {
    return { output.block.samples.data(), (int) output.block.samples.size() };
}

}

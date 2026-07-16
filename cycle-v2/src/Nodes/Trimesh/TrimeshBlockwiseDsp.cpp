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
    prepareRequest();
    if (mesh != nullptr && mesh->hasEnoughCubesForCrossSection()) {
        rasterizer.renderWaveformOnly(mesh, 0.f);
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

    sampleOutput(output);
}

void TrimeshBlockwiseDsp::prepareRequest() {
    auto& request = rasterizer.getRequest();
    request.cyclic = cyclic;
    request.xMinimum = cyclic ? -0.05f : 0.f;
    request.xMaximum = cyclic ? 1.05f : 1.f;
    request.morph = morph;
    request.primaryViewDimension = primaryViewAxis;
    request.scalingMode = Rasterization::PointScalingMode::Bipolar;
    request.calcDepthDimensions = false;
    request.lowResCurves = false;
    rasterizer.setWrapsEnds(cyclic);
}

void TrimeshBlockwiseDsp::sampleOutput(SignalPayload& output) {
    auto sampler = rasterizer.sampler();

    if (!sampler.isSampleable()) {
        return;
    }

    Buffer<float> dest = outputBuffer(output);
    const float delta = output.block.samples.size() > 0 ? 1.f / (float) output.block.samples.size() : 0.f;
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

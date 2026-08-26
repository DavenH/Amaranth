#include "Nodes/Trimesh/TrimeshBlockwiseDsp.h"

#include <Curve/Curve.h>
#include <Curve/Mesh/Vertex.h>
#include <Util/LogRegionMapping.h>

namespace CycleV2 {

namespace {

void ensureCurveTable() {
    if (Curve::table == nullptr) {
        Curve::calcTable();
    }
}

Rasterization::PointScalingMode scalingModeForDomain(PortDomain domain) {
    if (domain == PortDomain::TimeSignal
            || domain == PortDomain::SpectralPhaseSignal) {
        return Rasterization::PointScalingMode::Bipolar;
    }

    return Rasterization::PointScalingMode::Unipolar;
}

}

void TrimeshBlockwiseDsp::prepare(
        Mesh* meshToRender,
        const MorphPosition& morphPosition,
        int axis,
        bool shouldWrap,
        PortDomain domain) {
    preparedDomain = domain;
    setMesh(meshToRender);
    setMorphPosition(morphPosition);
    setPrimaryViewAxis(axis);
    setCyclic(shouldWrap);
    ensureCurveTable();
    configureGuideCurveSeeds(domain);
    if (mesh != nullptr && mesh->hasEnoughCubesForCrossSection()) {
        rasterizer.renderWaveform({ *mesh, createRequest(domain), 0.f });
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

void TrimeshBlockwiseDsp::setGuideCurveProvider(GuideCurveProvider* provider) {
    guideCurveProvider = provider;
    rasterizer.setGuideCurveProvider(provider);
}

void TrimeshBlockwiseDsp::setVoiceLifecycleSeed(uint32_t seed) {
    voiceLifecycleSeed = seed;
    hasVoiceLifecycleSeed = true;
}

void TrimeshBlockwiseDsp::setFrequencyMidiNote(int midiNote) {
    frequencyMidiNote = midiNote;
}

void TrimeshBlockwiseDsp::prepareSampling(size_t maximumFrameCount) {
    frequencyPositions.resize(maximumFrameCount);
    cachedFrequencyPositionCount = 0;
}

void TrimeshBlockwiseDsp::configureGuideCurveSeeds(PortDomain domain) {
    if (guideCurveProvider == nullptr) {
        return;
    }

    const auto* snapshot = dynamic_cast<const GuideCurveSnapshotProvider*>(guideCurveProvider);
    const int guideCount = snapshot != nullptr
            ? snapshot->size()
            : Rasterization::GuideCurveOffsetSeeds::capacity;
    const uint32_t stableSeed = GuideCurveSnapshotProvider::visualizationSeed(domain);
    const auto seed = hasVoiceLifecycleSeed
            ? Rasterization::GuideCurveSeed::voiceLifecycle(voiceLifecycleSeed)
            : Rasterization::GuideCurveSeed::visualization(stableSeed);
    rasterizer.updateOffsetSeeds(
            guideCount,
            GuideCurveProvider::tableSize,
            seed);
    rasterizer.setNoiseSeed((int) (seed.value % GuideCurveProvider::tableSize));
}

void TrimeshBlockwiseDsp::renderCycle(
        size_t frameCount,
        PortDomain domain,
        ChannelLayout channelLayout,
        SignalPayload& output) {
    prepare(mesh, morph, primaryViewAxis, cyclic, domain);
    renderPrepared(frameCount, domain, channelLayout, output);
}

void TrimeshBlockwiseDsp::renderPrepared(
        size_t frameCount,
        PortDomain domain,
        ChannelLayout channelLayout,
        SignalPayload& output) {
    preparedDomain = domain;
    output.block.samples.resize(frameCount);
    output.domain = domain;
    output.channelLayout = channelLayout;

    if (frameCount == 0) {
        return;
    }

    Buffer<float> primary = outputBuffer(output);
    primary.zero();

    if (mesh != nullptr && mesh->hasEnoughCubesForCrossSection()) {
        sampleOutput(primary);
    }

    if (output.isStereo()) {
        output.secondaryBlock.samples.resize(frameCount);
        primary.copyTo({
                output.secondaryBlock.samples.data(),
                (int) output.secondaryBlock.samples.size()
        });
    }
}

void TrimeshBlockwiseDsp::renderCycleInto(Buffer<float> output, PortDomain domain) {
    prepare(mesh, morph, primaryViewAxis, cyclic, domain);
    renderPreparedInto(output);
}

void TrimeshBlockwiseDsp::renderPreparedInto(Buffer<float> output) {
    output.zero();
    if (output.empty() || mesh == nullptr || !mesh->hasEnoughCubesForCrossSection()) {
        return;
    }

    sampleOutput(output);
}

Rasterization::RasterizationRequest TrimeshBlockwiseDsp::createRequest(
        PortDomain domain) const {
    Rasterization::RasterizationRequest request;
    request.cyclic = cyclic;
    request.xMinimum = cyclic ? -0.05f : 0.f;
    request.xMaximum = cyclic ? 1.05f : 1.f;
    request.morph = morph;
    request.primaryViewDimension = primaryViewAxis;
    request.scalingMode = scalingModeForDomain(domain);
    request.calcDepthDimensions = false;
    request.lowResCurves = false;
    return request;
}

void TrimeshBlockwiseDsp::sampleOutput(Buffer<float> dest) {
    auto sampler = rasterizer.sampler();

    if (!sampler.isSampleable()) {
        return;
    }

    const bool spectral = preparedDomain == PortDomain::SpectralMagnitudeSignal
            || preparedDomain == PortDomain::SpectralPhaseSignal;
    const Buffer<float> positions = spectral
            ? frequencyPositionsFor(dest.size())
            : Buffer<float>();
    const float delta = dest.size() > 0 ? 1.f / (float) dest.size() : 0.f;
    int currentIndex = sampler.initialIndex();

    for (int i = 0; i < dest.size(); ++i) {
        const float phase = spectral ? positions[i] : (float) i * delta;

        if (sampler.isSampleableAt(phase)) {
            dest[i] = sampler.sampleAt(phase, currentIndex);
        }
    }
}

Buffer<float> TrimeshBlockwiseDsp::frequencyPositionsFor(int size) {
    if ((int) frequencyPositions.size() < size) {
        frequencyPositions.resize((size_t) size);
        cachedFrequencyPositionCount = 0;
    }

    Buffer<float> positions(frequencyPositions.data(), size);
    if (cachedFrequencyMidiNote != frequencyMidiNote
            || cachedFrequencyPositionCount != size) {
        LogRegionMapping(frequencyMidiNote).fillDisplayUnits(positions);
        cachedFrequencyMidiNote = frequencyMidiNote;
        cachedFrequencyPositionCount = size;
    }
    return positions;
}

Buffer<float> TrimeshBlockwiseDsp::outputBuffer(SignalPayload& output) const {
    return { output.block.samples.data(), (int) output.block.samples.size() };
}

}

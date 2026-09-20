#include "Runtime/SpectralFrameSourceRenderer.h"

#include "Graph/GraphCompiler.h"
#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Nodes/Trimesh/Model/TrimeshMeshDeltaOverlay.h"
#include "Runtime/PreparedCycleEnvelopeBank.h"
#include "Runtime/PreparedOscillatorRegion.h"
#include "Runtime/PreparedTrimeshMorphBinding.h"
#include "Runtime/TrimeshMorphResolver.h"

#include <Array/ScopedAlloc.h>
#include <Audio/CycleDsp/OscillatorLaneRasterizer.h>
#include <Audio/CycleDsp/SpectralLayerCore.h>
#include <Curve/Curve.h>
#include <Curve/Rasterization/Rasterizer/VoiceRasterizer.h>
#include <Util/LogRegionMapping.h>

#include <array>

namespace CycleV2 {

namespace {

class PreparedSourceCore final {
public:
    bool prepare(
            const GraphExecutionPlan& plan,
            const GraphExecutionStep& step) {
        configuration = std::dynamic_pointer_cast<const TrimeshConfiguration>(
                step.configuration.value);
        if (configuration == nullptr
                || configuration->mesh == nullptr
                || step.outputs.empty()) {
            return false;
        }
        outputDomain = step.outputs.front().domain;
        morphBinding.bind(plan, step);
        reset();
        return true;
    }

    void reset() {
        morphResolver.reset(
                configuration != nullptr ? configuration->morph : MorphPosition(),
                true);
    }

    MorphPosition resolveMorph(const SpectralFrameSourceRenderRequest& request) {
        CycleDsp::ScopedSourceRenderStage morphStage(
                request.performance,
                CycleDsp::SourceRenderStage::MorphResolution);
        const TrimeshMorphInputs inputs = request.context != nullptr
                ? morphBinding.inputsFor(
                        *request.context,
                        request.blockSampleOffset,
                        request.voiceSamplePosition,
                        request.cycleEnvelopes)
                : TrimeshMorphInputs {};
        return morphResolver.resolve(
                inputs,
                configuration->morph,
                outputDomain,
                configuration->primaryViewAxis,
                request.blockSampleOffset,
                request.elapsedSamples,
                request.context != nullptr ? request.context->timing.sampleRate : 44100.0,
                configuration->scratchSourceEnabled);
    }

    void captureRaster(
            const SpectralFrameSourceRenderRequest& request,
            const MorphPosition& morph,
            Buffer<float> values) const {
        if (outputDomain != PortDomain::TimeSignal
                && outputDomain != PortDomain::SpectralMagnitudeSignal
                && outputDomain != PortDomain::SpectralPhaseSignal) {
            return;
        }
        const auto stage = outputDomain == PortDomain::TimeSignal
                ? CycleDsp::SpectralStage::TimeRaster
                : outputDomain == PortDomain::SpectralMagnitudeSignal
                        ? CycleDsp::SpectralStage::MagnitudeRaster
                        : CycleDsp::SpectralStage::PhaseRaster;
        std::array<float, 3> morphValues {
                morph.time.getCurrentValue(),
                morph.red.getCurrentValue(),
                morph.blue.getCurrentValue()
        };
        const Buffer<float> capturedValues = outputDomain == PortDomain::TimeSignal
                ? values
                : values.section(1, request.activeHarmonicCount);
        for (int channel = 0; channel < 2; ++channel) {
            request.capture->capture(
                    stage,
                    channel,
                    capturedValues,
                    { morphValues.data(), (int) morphValues.size() });
        }
    }

    PortDomain outputDomain { PortDomain::TimeSignal };
    std::shared_ptr<const TrimeshConfiguration> configuration;

private:
    PreparedTrimeshMorphBinding morphBinding;
    TrimeshMorphResolver morphResolver;
};

class TimeFrameSourceRenderer final : public SpectralFrameSourceRenderer {
public:
    bool prepare(
            const GraphExecutionPlan& plan,
            const GraphExecutionStep& step,
            int maximumFrameSize) {
        if (!core.prepare(plan, step)) {
            return false;
        }
        rasterizer.setCalcDepthDimensions(false);
        rasterizer.setPrepareIntegrals(false);
        rasterizer.setGuideCurveProvider(core.configuration->guideCurveProvider.get());
        rasterizer.setCubeResolver(
                core.configuration->deltaOverlay.get(),
                core.configuration->deltaOverlay != nullptr
                        ? &TrimeshMeshDeltaOverlay::resolveFromContext
                        : nullptr);
        rasterizer.setScalingMode(Rasterization::PointScalingMode::Bipolar);
        rasterizer.prepare(
                Rasterization::VoiceRasterizerPreparation::forMesh(
                        *const_cast<Mesh*>(core.configuration->mesh.get())),
                { &state });
        cachedMemory.resize(2 * maximumFrameSize);
        cachedMemory.resetPlacement();
        for (auto& frame : cachedFrames) {
            frame = cachedMemory.place(maximumFrameSize);
        }
        reset();
        return true;
    }

    void reset() override {
        cachedFrameSize = 0;
        core.reset();
        state.reset();
        rasterizer.orphanOldVerts();
    }

    void setVoiceLifecycleSeeds(
            uint32_t timeSeed,
            uint32_t,
            uint32_t) override {
        rasterizer.updateOffsetSeeds(
                (int) core.configuration->guideAssignmentCount,
                GuideCurveProvider::tableSize,
                Rasterization::GuideCurveSeed::voiceLifecycle(timeSeed));
    }

    void render(
            const SpectralFrameSourceRenderRequest& request,
            Buffer<float> left,
            Buffer<float> right) override {
        if (!request.refreshTimeSource && cachedFrameSize == request.frameSize) {
            cachedFrames[0].withSize(request.frameSize).copyTo(left);
            cachedFrames[1].withSize(request.frameSize).copyTo(right);
            return;
        }
        const MorphPosition morph = core.resolveMorph(request);
        if (!core.configuration->enabled) {
            left.zero();
            right.zero();
        } else {
            renderEnabled(request, morph, left, right);
        }
        left.copyTo(cachedFrames[0].withSize(request.frameSize));
        right.copyTo(cachedFrames[1].withSize(request.frameSize));
        cachedFrameSize = request.frameSize;
    }

    bool isTimeSource() const override { return true; }
    bool hasActiveSpectralMesh() const override { return false; }

private:
    void renderEnabled(
            const SpectralFrameSourceRenderRequest& request,
            const MorphPosition& morph,
            Buffer<float> left,
            Buffer<float> right) {
        CycleDsp::OscillatorLaneRasterizer::renderFixedFrame(
                rasterizer,
                {
                        const_cast<Mesh*>(core.configuration->mesh.get()),
                        morph,
                        0.f,
                        request.random->nextInt(GuideCurveProvider::tableSize)
                },
                left,
                request.performance);
        core.captureRaster(request, morph, left);
        {
            CycleDsp::ScopedSourceRenderStage gainStage(
                    request.performance, CycleDsp::SourceRenderStage::Gain);
            left.mul(core.configuration->gain);
        }
        CycleDsp::ScopedSourceRenderStage copyStage(
                request.performance, CycleDsp::SourceRenderStage::StereoCopy);
        left.copyTo(right);
    }

    int cachedFrameSize {};
    PreparedSourceCore core;
    Rasterization::VoiceCycleState state;
    Rasterization::VoiceRasterizer rasterizer;
    ScopedAlloc<float> cachedMemory;
    std::array<Buffer<float>, 2> cachedFrames;
};

class SpectralMeshSourceRenderer final : public SpectralFrameSourceRenderer {
public:
    bool prepare(
            const GraphExecutionPlan& plan,
            const GraphExecutionStep& step,
            int maximumFrameSize) {
        if (!core.prepare(plan, step)) {
            return false;
        }
        auto* mesh = const_cast<Mesh*>(core.configuration->mesh.get());
        activeMesh = core.configuration->enabled
                && mesh->hasEnoughCubesForCrossSection();
        rasterizer.setGuideCurveProvider(core.configuration->guideCurveProvider.get());
        rasterizer.setDeltaOverlay(core.configuration->deltaOverlay);
        rasterizer.prepare(
                mesh,
                core.configuration->morph,
                core.configuration->primaryViewAxis,
                false,
                core.outputDomain);
        rasterizer.prepareSampling((size_t) maximumFrameSize);
        return true;
    }

    void reset() override { core.reset(); }

    void setVoiceLifecycleSeeds(
            uint32_t,
            uint32_t magnitudeSeed,
            uint32_t phaseSeed) override {
        rasterizer.setVoiceLifecycleSeed(
                core.outputDomain == PortDomain::SpectralPhaseSignal
                        ? phaseSeed
                        : magnitudeSeed,
                1);
    }

    void render(
            const SpectralFrameSourceRenderRequest& request,
            Buffer<float> left,
            Buffer<float> right) override {
        const MorphPosition morph = core.resolveMorph(request);
        if (!core.configuration->enabled) {
            left.zero();
            right.zero();
            return;
        }
        rasterize(request, morph, left);
        core.captureRaster(request, morph, left);
        applyGainAndPhase(request, left);
        CycleDsp::ScopedSourceRenderStage copyStage(
                request.performance, CycleDsp::SourceRenderStage::StereoCopy);
        left.copyTo(right);
    }

    bool isTimeSource() const override { return false; }
    bool hasActiveSpectralMesh() const override { return activeMesh; }

private:
    void rasterize(
            const SpectralFrameSourceRenderRequest& request,
            const MorphPosition& morph,
            Buffer<float> output) {
        {
            CycleDsp::ScopedSourceRenderStage rasterStage(
                    request.performance, CycleDsp::SourceRenderStage::Rasterization);
            rasterizer.setFrequencyMidiNote(
                    request.midiNote + LogRegionMapping::legacyMidiNoteBias);
            rasterizer.setMorphPosition(morph);
            rasterizer.rasterizePrepared(
                    request.random->nextInt(GuideCurveProvider::tableSize),
                    request.performance == nullptr
                            ? nullptr
                            : &request.performance->waveform);
        }
        CycleDsp::ScopedSourceRenderStage samplingStage(
                request.performance, CycleDsp::SourceRenderStage::Sampling);
        output.zero();
        rasterizer.renderPreparedHarmonicsInto(output.section(1, output.size() - 1));
    }

    void applyGainAndPhase(
            const SpectralFrameSourceRenderRequest& request,
            Buffer<float> output) const {
        CycleDsp::ScopedSourceRenderStage gainStage(
                request.performance, CycleDsp::SourceRenderStage::Gain);
        output.mul(core.configuration->gain);
        if (core.outputDomain != PortDomain::SpectralPhaseSignal) {
            return;
        }
        output.mul(CycleDsp::SpectralLayerCore::phaseOffsetScale(
                core.configuration->range) * MathConstants<float>::twoPi);
        for (int channel = 0; channel < 2; ++channel) {
            request.capture->capture(
                    CycleDsp::SpectralStage::PhaseOperand,
                    channel,
                    output.section(1, request.activeHarmonicCount));
        }
        output.section(1, output.size() - 1).mul(
                request.phaseHarmonicScale.withSize(output.size() - 1));
    }

    bool activeMesh {};
    PreparedSourceCore core;
    TrimeshBlockwiseDsp rasterizer;
};

}

std::unique_ptr<SpectralFrameSourceRenderer> SpectralFrameSourceRenderer::create(
        const GraphExecutionPlan& plan,
        const GraphExecutionStep& step,
        int maximumFrameSize) {
    if (step.outputs.empty()) {
        return {};
    }
    if (Curve::table == nullptr) {
        Curve::calcTable();
    }
    if (step.outputs.front().domain == PortDomain::TimeSignal) {
        auto source = std::make_unique<TimeFrameSourceRenderer>();
        if (!source->prepare(plan, step, maximumFrameSize)) {
            return {};
        }
        return source;
    }
    auto source = std::make_unique<SpectralMeshSourceRenderer>();
    if (!source->prepare(plan, step, maximumFrameSize)) {
        return {};
    }
    return source;
}

}

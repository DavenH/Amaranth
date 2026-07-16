#include <algorithm>

#include <Curve/Mesh/Intercept.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Policies/Core/InterceptPolicies.h>
#include <Curve/Rasterization/Policies/Curves/CurvePolicies.h>
#include <Curve/Rasterization/Policies/Curves/WaveformBakePolicy.h>
#include <Curve/Rasterization/Policies/Voice/VoicePolicies.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>

#include "VoiceRasterizer.h"

namespace Rasterization {

VoiceRasterizer::VoiceRasterizer(float minimumLineLength) :
        chainResult()
    ,   chainReduction()
    ,   initialAdvancement(minimumLineLength * 1.1f) {
    auto& request = getRequest();
    request.overrideDimension = true;
    request.scalingMode = Rasterization::PointScalingMode::Bipolar;
    request.calcDepthDimensions = false;
    rasterizerData.paddingSize = getPaddingSize();
    rasterizerData.wrapsVertices = request.cyclic;
    chainResult.paddingSize = 2;
    updateChainBuffers(2048);
}

const RenderResult& VoiceRasterizer::renderOrdinary(Mesh* mesh, float phase) {
    activeOutput = ActiveOutput::Ordinary;
    renderWaveformOnly(mesh, phase);
    return result();
}

const RenderResult& VoiceRasterizer::renderChained(float oscPhase) {
    activeOutput = ActiveOutput::Chained;

    if (mesh == nullptr || mesh->getNumCubes() == 0 || state == nullptr) {
        cleanUp();
        activeOutput = ActiveOutput::Chained;
        return chainResult;
    }

    Rasterization::VoiceChainingPolicy chainingPolicy(&chainResult.needsResorting);
    chainingPolicy.beginCall(*state, chainResult.intercepts);

    const auto& output = renderVoiceSlice(oscPhase);

    chainingPolicy.publishNextIntercepts(
            output,
            *state,
            [this](std::vector<Intercept>& intercepts) { restrictIntercepts(intercepts); });

    if (!chainingPolicy.canBuildChainedCurves(*state, chainResult.intercepts)) {
        ++state->callCount;
        markChainedWaveformUnsampleable();
        return chainResult;
    }

    if (state->callCount == 0) {
        state->advancement = initialAdvancement;
    }

    if (state->callCount > 0) {
        chainResult.paddingSize = Rasterization::VoiceChainedPaddingPolicy().build(
                chainResult.intercepts,
                state->backIcpts,
                *state,
                chainResult.curves,
                getRequest().interceptPadding);

        bakeChainedWaveform();
    }

    ++state->callCount;
    return chainResult;
}

void VoiceRasterizer::orphanOldVerts() {
    chainResult.intercepts.clear();
}

void VoiceRasterizer::cleanUp() {
    clearTrilinearOutput();
    cleanChainedOutput();
}

bool VoiceRasterizer::currentWaveformIsSampleable() const {
    return activeOutput == ActiveOutput::Chained
           ? chainResult.sampleable
           : Rasterization::TrilinearMeshRasterizer::sampler().isSampleable();
}

WaveformBuffers VoiceRasterizer::currentWaveform() const {
    return activeOutput == ActiveOutput::Chained ? chainResult.waveform : waveform();
}

void VoiceRasterizer::bakeChainedWaveform() {
    if (chainResult.intercepts.size() < 2) {
        cleanChainedOutput();
        return;
    }

    Rasterization::VoiceCurveResolutionPolicy().apply(chainResult.curves);
    Rasterization::CurveWaveformPreparationPolicy().apply(chainResult.curves);

    Rasterization::WaveformBakePolicy::Context context;
    context.lowResCurves = getRequest().lowResCurves;
    context.decoupleComponentDfrms = getRequest().decoupleComponentDeforms;
    context.noiseSeed = getRequest().noiseSeed;
    context.morph = getRequest().morph;
    context.guideCurveProvider = getGuideCurveProvider();
    context.guideCurveRegions = &chainResult.guideCurveRegions;
    context.offsetSeeds = nullptr;

    chainResult.sampleable = Rasterization::WaveformBakePolicy().build(
            chainResult.curves,
            context,
            [this](int totalRes) {
                updateChainBuffers(totalRes);
                return Rasterization::WaveformBufferRefs(chainResult.waveform);
            });
}

void VoiceRasterizer::cleanChainedOutput() {
    chainResult.clear();
    chainResult.paddingSize = 2;
}

const RenderResult& VoiceRasterizer::renderVoiceSlice(float oscPhase) {
    sliceResult.clear();

    if (mesh == nullptr || mesh->getNumCubes() == 0 || state == nullptr) {
        return sliceResult;
    }

    float voiceTime = jmin(1.f, getRequest().morph.time + state->advancement);
    auto guideApplier = createGuideCurveApplier(chainReduction, &chainResult.needsResorting);

    auto& cubes = mesh->getCubes();
    for (int i = 0; i < (int) cubes.size(); ++i) {
        appendVoiceCubeIntercept(cubes[i], voiceTime, oscPhase, guideApplier, sliceResult.intercepts);
    }

    std::sort(sliceResult.intercepts.begin(), sliceResult.intercepts.end());
    sliceResult.sampleable = sliceResult.intercepts.size() >= 2;

    return sliceResult;
}

void VoiceRasterizer::appendVoiceCubeIntercept(
        VertCube* cube,
        float voiceTime,
        float oscPhase,
        GuideCurveApplier& applyGuide,
        std::vector<Intercept>& intercepts) {
    voiceSlicer.slice(
            *cube,
            Vertex::Time,
            chainReduction,
            getRequest().morph.withTime(voiceTime));

    Vertex* a = &chainReduction.v0;
    Vertex* b = &chainReduction.v1;
    Vertex* vertex = &chainReduction.v;

    Rasterization::VoicePointPositionPolicy::Context pointContext;
    pointContext.voiceTime = voiceTime;
    pointContext.oscPhase = oscPhase;

    auto point = voicePointPositionPolicy.resolve(
            pointContext,
            chainReduction.pointOverlaps,
            a,
            b,
            vertex);

    if (!point.intersects) {
        return;
    }

    Intercept intercept(point.phase, 2.f * vertex->values[Vertex::Amp] - 1.f, cube, 0);
    intercept.shp = vertex->values[Vertex::Curve];
    intercept.adjustedX = intercept.x;

    applyGuide(intercept, getRequest().morph);
    intercepts.push_back(intercept);
}

void VoiceRasterizer::markChainedWaveformUnsampleable() {
    chainResult.waveform.waveX.nullify();
    chainResult.waveform.waveY.nullify();
    chainResult.sampleable = false;
}

void VoiceRasterizer::publishCurrentResult() {
    Rasterization::RasterizerSnapshotSource source;

    if (activeOutput == ActiveOutput::Chained) {
        source.intercepts = &chainResult.intercepts;
        source.colorPoints = &chainResult.colorPoints;
        source.curves = &chainResult.curves;
        source.waveform = chainResult.waveform;
        source.paddingSize = chainResult.paddingSize;
        source.wrapsVertices = getRequest().cyclic;
        source.sampleable = chainResult.sampleable;
    } else {
        source = createSnapshotSource();
    }

    Rasterization::BaseRasterizer::publishSnapshot(source);
}

void VoiceRasterizer::updateChainBuffers(int size) {
    chainResult.waveform.place(chainResult.waveformMemory, size);
}

void VoiceRasterizer::restrictIntercepts(std::vector<Intercept>& intercepts) {
    if (intercepts.empty()) {
        return;
    }

    Rasterization::InterceptRestrictionPolicy::Context context;
    context.cyclic = getRequest().cyclic;
    context.minimumX = getRequest().xMinimum;
    context.maximumX = getRequest().xMaximum;

    Rasterization::InterceptRestrictionPolicy(context).restrict(intercepts);
}

}

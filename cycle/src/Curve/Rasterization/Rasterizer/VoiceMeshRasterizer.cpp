#include <algorithm>

#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Curve/Mesh/Intercept.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Policies/Core/InterceptPolicies.h>
#include <Curve/Rasterization/Policies/Curves/CurvePolicies.h>
#include <Curve/Rasterization/Policies/Curves/WaveformBakePolicy.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Definitions.h>

#include "VoiceMeshRasterizer.h"
#include <Curve/CycleState.h>
#include <Curve/Rasterization/Policies/Voice/VoicePolicies.h>
#include <Util/CycleEnums.h>


VoiceMeshRasterizer::VoiceMeshRasterizer(SingletonRepo* repo) :
		SingletonAccessor(repo, "VoiceMeshRasterizer")
    ,   chainResult()
    ,   chainReduction()
    ,   chainPaddingSize(2)
    ,   chainUnsampleable(true)
    ,   chainNeedsResorting(false)
    ,   chainedOutputActive(false)
    ,   state(nullptr) {
    auto& request = getRequest();
    request.overrideDimension = true;
    request.scalingMode = Rasterization::PointScalingMode::Bipolar;
    request.calcDepthDimensions = false;
    rasterizerData.paddingSize = getPaddingSize();
    rasterizerData.wrapsVertices = request.cyclic;
    updateChainBuffers(2048);
}

void VoiceMeshRasterizer::updateChainedWaveform(float oscPhase) {
    if (mesh == nullptr || mesh->getNumCubes() == 0 || state == nullptr) {
        cleanUp();
        return;
    }

    chainedOutputActive = true;

    Rasterization::VoiceChainingPolicy chainingPolicy(&chainNeedsResorting);
    chainingPolicy.beginCall(*state, chainResult.intercepts);

    auto output = renderVoiceSlice(oscPhase);

    chainingPolicy.publishNextIntercepts(
            output,
            *state,
            [this](std::vector<Intercept>& intercepts) { restrictIntercepts(intercepts); });

    if (!chainingPolicy.canBuildChainedCurves(*state, chainResult.intercepts)) {
        ++state->callCount;
        markChainedWaveformUnsampleable();
        publishSnapshot();
        return;
    }

    if (state->callCount == 0) {
        state->advancement = getRealConstant(MinLineLength) * 1.1f;
    }

    if (state->callCount > 0) {
        chainPaddingSize = Rasterization::VoiceChainedPaddingPolicy().build(
                chainResult.intercepts,
                state->backIcpts,
                *state,
                chainResult.curves,
                getRequest().interceptPadding);

        bakeChainedWaveform();
    }

    ++state->callCount;
    publishSnapshot();
}

void VoiceMeshRasterizer::orphanOldVerts() {
    chainResult.intercepts.clear();
}

void VoiceMeshRasterizer::cleanUp() {
    clearTrilinearOutput();
    cleanChainedOutput();
    publishSnapshot();
}

bool VoiceMeshRasterizer::currentWaveformIsSampleable() const {
    return chainedOutputActive
           ? Rasterization::WaveformSampler::isSampleable(chainResult.waveform)
           : Rasterization::TrilinearMeshRasterizer::sampler().isSampleable();
}

Rasterization::WaveformBuffers VoiceMeshRasterizer::currentWaveform() const {
    return chainedOutputActive ? chainResult.waveform : waveform();
}

void VoiceMeshRasterizer::bakeChainedWaveform() {
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

    chainUnsampleable = !Rasterization::WaveformBakePolicy().build(
            chainResult.curves,
            context,
            [this](int totalRes) {
                updateChainBuffers(totalRes);
                return Rasterization::WaveformBufferRefs(chainResult.waveform);
            });
}

void VoiceMeshRasterizer::cleanChainedOutput() {
    chainResult.clear();
    chainPaddingSize = 2;
    chainUnsampleable = true;
    chainNeedsResorting = false;
}

Rasterization::RenderResult VoiceMeshRasterizer::renderVoiceSlice(float oscPhase) {
    Rasterization::RenderResult output;

    if (mesh == nullptr || mesh->getNumCubes() == 0 || state == nullptr) {
        return output;
    }

    float voiceTime = jmin(1.f, getRequest().morph.time + state->advancement);
    auto guideApplier = createGuideCurveApplier(chainReduction, &chainNeedsResorting);

    auto& cubes = mesh->getCubes();
    for (int i = 0; i < (int) cubes.size(); ++i) {
        appendVoiceCubeIntercept(cubes[i], voiceTime, oscPhase, guideApplier, output.intercepts);
    }

    std::sort(output.intercepts.begin(), output.intercepts.end());
    output.sampleable = output.intercepts.size() >= 2;

    return output;
}

void VoiceMeshRasterizer::appendVoiceCubeIntercept(
        VertCube* cube,
        float voiceTime,
        float oscPhase,
        Rasterization::GuideCurveApplier& applyGuide,
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

void VoiceMeshRasterizer::markChainedWaveformUnsampleable() {
    chainResult.waveform.waveX.nullify();
    chainResult.waveform.waveY.nullify();
    chainUnsampleable = true;
}

void VoiceMeshRasterizer::publishSnapshot() {
    Rasterization::RasterizerSnapshotSource source;

    if (chainedOutputActive) {
        source.intercepts = &chainResult.intercepts;
        source.colorPoints = &chainResult.colorPoints;
        source.curves = &chainResult.curves;
        source.waveform = chainResult.waveform;
        source.paddingSize = chainPaddingSize;
        source.wrapsVertices = getRequest().cyclic;
    } else {
        source = createSnapshotSource();
    }

    Rasterization::BaseRasterizer::publishSnapshot(source);
}

void VoiceMeshRasterizer::updateChainBuffers(int size) {
    chainResult.waveform.place(chainResult.waveformMemory, size);
}

void VoiceMeshRasterizer::restrictIntercepts(std::vector<Intercept>& intercepts) {
    if (intercepts.empty()) {
        return;
    }

    Rasterization::InterceptRestrictionPolicy::Context context;
    context.cyclic = getRequest().cyclic;
    context.minimumX = getRequest().xMinimum;
    context.maximumX = getRequest().xMaximum;

    Rasterization::InterceptRestrictionPolicy(context).restrict(intercepts);
}

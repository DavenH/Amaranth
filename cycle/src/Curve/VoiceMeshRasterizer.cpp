#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Curve/Intercept.h>
#include <Curve/Mesh.h>
#include <Curve/Rasterization/Builders/RasterizerSnapshotBuilder.h>
#include <Curve/Rasterization/Policies/Core/InterceptPolicies.h>
#include <Curve/Rasterization/Policies/Curves/CurvePolicies.h>
#include <Curve/Rasterization/Policies/Curves/WaveformBakePolicy.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Definitions.h>

#include "VoiceMeshRasterizer.h"
#include "CycleState.h"
#include "Rasterization/Pipelines/VoiceSlicePipeline.h"
#include "Rasterization/Policies/Voice/VoicePolicies.h"
#include "../Util/CycleEnums.h"


VoiceMeshRasterizer::VoiceMeshRasterizer(SingletonRepo* repo) :
		SingletonAccessor(repo, "VoiceMeshRasterizer")
    ,   mesh(nullptr)
    ,   rasterizer()
    ,   chainResult()
    ,   rasterizerData()
    ,   chainReduction()
    ,   chainPaddingSize(2)
    ,   chainUnsampleable(true)
    ,   chainNeedsResorting(false)
    ,   chainedOutputActive(false)
    ,   state(nullptr) {
    auto& request = rasterizer.getRequest();
    request.overrideDimension = true;
    request.scalingMode = Rasterization::PointScalingMode::Bipolar;
    request.calcDepthDimensions = false;
    updateChainBuffers(2048);
}

void VoiceMeshRasterizer::calcCrossPointsChaining(float oscPhase) {
    if (mesh == nullptr || mesh->getNumCubes() == 0 || state == nullptr) {
        cleanUp();
        return;
    }

    chainedOutputActive = true;

    Cycle::Rasterization::VoiceChainingPolicy chainingPolicy(&chainNeedsResorting);
    chainingPolicy.beginCall(*state, chainResult.intercepts);

    auto output = Cycle::Rasterization::VoiceSlicePipeline().render(
            mesh,
            rasterizer.getRequest().morph,
            state->advancement,
            oscPhase,
            rasterizer.createGuideCurveApplier(chainReduction, &chainNeedsResorting),
            chainReduction);

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
        chainPaddingSize = Cycle::Rasterization::VoiceChainedPaddingPolicy().build(
                chainResult.intercepts,
                state->backIcpts,
                *state,
                chainResult.curves,
                rasterizer.getRequest().interceptPadding);

        bakeChainedWaveform();
    }

    ++state->callCount;
    publishSnapshot();
}

void VoiceMeshRasterizer::orphanOldVerts() {
    chainResult.intercepts.clear();
}

void VoiceMeshRasterizer::calcCrossPoints(Mesh* mesh, float oscPhase) {
    chainedOutputActive = false;

    if (mesh == nullptr || mesh->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    setMesh(mesh);
    rasterizer.render(mesh, oscPhase);
    publishSnapshot();
}

void VoiceMeshRasterizer::cleanUp() {
    rasterizer.clean();
    cleanChainedOutput();
    publishSnapshot();
}

void VoiceMeshRasterizer::performUpdate(UpdateType updateType) {
    if (updateType == Update) {
        calcCrossPoints(mesh, 0.f);
    }
}

bool VoiceMeshRasterizer::hasEnoughCubesForCrossSection() {
    return mesh != nullptr && mesh->hasEnoughCubesForCrossSection();
}

bool VoiceMeshRasterizer::isSampleable() {
    return chainedOutputActive
           ? Rasterization::WaveformSampler::isSampleable(chainResult.waveform)
           : rasterizer.isSampleable();
}

bool VoiceMeshRasterizer::isSampleableAt(float x) {
    return chainedOutputActive
           ? Rasterization::WaveformSampler::isSampleableAt(chainResult.waveform, x)
           : rasterizer.isSampleableAt(x);
}

float VoiceMeshRasterizer::sampleAt(double angle) {
    if (!chainedOutputActive) {
        return rasterizer.sampleAt(angle);
    }

    return Rasterization::WaveformSampler::sampleAt(currentWaveform(), chainUnsampleable, angle);
}

float VoiceMeshRasterizer::sampleAt(double angle, int& currentIndex) {
    if (!chainedOutputActive) {
        return rasterizer.sampleAt(angle, currentIndex);
    }

    return Rasterization::WaveformSampler::sampleAt(currentWaveform(), chainUnsampleable, angle, currentIndex);
}

float VoiceMeshRasterizer::samplePerfectly(double delta, Buffer<float> buffer, double phase) {
    if (!chainedOutputActive) {
        return rasterizer.samplePerfectly(delta, buffer, phase);
    }

    return Rasterization::WaveformSampler::samplePerfectly(currentWaveform(), delta, buffer, phase);
}

int VoiceMeshRasterizer::getPaddingSize() const {
    return chainedOutputActive ? chainPaddingSize : rasterizer.getPaddingSize();
}

Rasterization::WaveformBuffers VoiceMeshRasterizer::currentWaveform() const {
    return chainedOutputActive ? chainResult.waveform : rasterizer.waveform();
}

void VoiceMeshRasterizer::bakeChainedWaveform() {
    if (chainResult.intercepts.size() < 2) {
        cleanChainedOutput();
        return;
    }

    Cycle::Rasterization::VoiceCurveResolutionPolicy().apply(chainResult.curves);
    Rasterization::CurveWaveformPreparationPolicy().apply(chainResult.curves);

    Rasterization::WaveformBakePolicy::Context context;
    context.lowResCurves = rasterizer.getRequest().lowResCurves;
    context.decoupleComponentDfrms = rasterizer.getRequest().decoupleComponentDeforms;
    context.noiseSeed = rasterizer.getRequest().noiseSeed;
    context.morph = rasterizer.getRequest().morph;
    context.guideCurveProvider = rasterizer.getGuideCurveProvider();
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
    } else {
        source = rasterizer.createSnapshotSource();
    }

    Rasterization::RasterizerSnapshotBuilder().publish(rasterizerData, source);
}

void VoiceMeshRasterizer::updateChainBuffers(int size) {
    chainResult.waveform.place(chainResult.waveformMemory, size);
}

void VoiceMeshRasterizer::restrictIntercepts(std::vector<Intercept>& intercepts) {
    if (intercepts.empty()) {
        return;
    }

    Rasterization::InterceptRestrictionPolicy::Context context;
    context.cyclic = rasterizer.getRequest().cyclic;
    context.minimumX = rasterizer.getRequest().xMinimum;
    context.maximumX = rasterizer.getRequest().xMaximum;

    Rasterization::InterceptRestrictionPolicy(context).restrict(intercepts);
}

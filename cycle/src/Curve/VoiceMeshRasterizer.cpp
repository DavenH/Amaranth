#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Curve/Intercept.h>
#include <Curve/Mesh.h>
#include <Curve/Rasterization/Builders/RasterizerSnapshotBuilder.h>
#include <Curve/Rasterization/Builders/TransferTable.h>
#include <Curve/Rasterization/Policies/Core/InterceptRestrictionPolicy.h>
#include <Curve/Rasterization/Policies/Core/RasterizerCleanupPolicy.h>
#include <Curve/Rasterization/Policies/Curves/CurveWaveformPreparationPolicy.h>
#include <Curve/Rasterization/Policies/Curves/WaveformBakePolicy.h>
#include <Curve/Rasterization/Policies/Curves/WaveformBuildPolicy.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Definitions.h>

#include "VoiceMeshRasterizer.h"
#include "CycleState.h"
#include "Rasterization/Pipelines/VoiceRasterizationPipeline.h"
#include "Rasterization/Policies/Voice/VoiceWaveformUpdatePolicy.h"
#include "../Util/CycleEnums.h"


VoiceMeshRasterizer::VoiceMeshRasterizer(SingletonRepo* repo) :
		SingletonAccessor(repo, "VoiceMeshRasterizer")
    ,   mesh(nullptr)
    ,   rasterizer()
    ,   chainStorage()
    ,   rasterizerData()
    ,   chainWaveformMemory()
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
    Cycle::Rasterization::VoiceRasterizationPipeline::Context context;
    context.mesh = mesh;
    context.state = state;
    context.morph = rasterizer.getRequest().morph;
    context.runtime = createChainRuntime();
    context.reductionData = &chainReduction;
    context.oscPhase = oscPhase;
    context.interceptPadding = rasterizer.getRequest().interceptPadding;
    context.initialAdvancement = getRealConstant(MinLineLength) * 1.1f;

    chainedOutputActive = true;

    bool rendered = Cycle::Rasterization::VoiceRasterizationPipeline().render(
            context,
            rasterizer.createGuideCurveApplier(chainReduction, &chainNeedsResorting),
            [this](std::vector<Intercept>& intercepts) { restrictIntercepts(intercepts); },
            [](::Rasterization::RasterizerRuntime runtime) {
                ::Rasterization::RasterizerCleanupPolicy::markWaveformUnsampleable(runtime);
            },
            [this]() {
                bakeChainedWaveform();
            });

    if (!rendered) {
        cleanUp();
        return;
    }

    publishSnapshot();
}

void VoiceMeshRasterizer::orphanOldVerts() {
    createChainRuntime().clearIntercepts();
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
    Rasterization::RasterizerCleanupPolicy().clean(createChainRuntime());
    chainStorage.curves.guideCurveRegions.clear();
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
           ? Rasterization::WaveformSampler::isSampleable(chainStorage.waveform.waveform)
           : rasterizer.isSampleable();
}

bool VoiceMeshRasterizer::isSampleableAt(float x) {
    return chainedOutputActive
           ? Rasterization::WaveformSampler::isSampleableAt(chainStorage.waveform.waveform, x)
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

Rasterization::RasterizerRuntime VoiceMeshRasterizer::createChainRuntime() {
    Rasterization::RasterizerRuntime runtime;
    runtime.intercepts = &chainStorage.intercepts.intercepts;
    runtime.curves = &chainStorage.curves.curves;
    runtime.frontPadding = &chainStorage.intercepts.frontPadding;
    runtime.backPadding = &chainStorage.intercepts.backPadding;
    runtime.colorPoints = &chainStorage.intercepts.colorPoints;
    runtime.waveform = Rasterization::WaveformBufferRefs(chainStorage.waveform.waveform);
    runtime.paddingSize = &chainPaddingSize;
    runtime.unsampleable = &chainUnsampleable;
    runtime.needsResorting = &chainNeedsResorting;

    return runtime;
}

Rasterization::WaveformBuffers VoiceMeshRasterizer::currentWaveform() const {
    return chainedOutputActive ? chainStorage.waveform.waveform : rasterizer.waveform();
}

void VoiceMeshRasterizer::bakeChainedWaveform() {
    Cycle::Rasterization::VoiceWaveformUpdatePolicy().update(
            createChainRuntime(),
            [this]() {
                Rasterization::RasterizerCleanupPolicy().clean(createChainRuntime());
                chainStorage.curves.guideCurveRegions.clear();
            },
            [this]() {
                Rasterization::CurveWaveformPreparationPolicy().apply(chainStorage.curves.curves);
            },
            [this]() {
                Rasterization::WaveformBakePolicy::Context context;
                context.lowResCurves = rasterizer.getRequest().lowResCurves;
                context.decoupleComponentDfrms = rasterizer.getRequest().decoupleComponentDeforms;
                context.noiseSeed = rasterizer.getRequest().noiseSeed;
                context.morph = rasterizer.getRequest().morph;
                context.guideCurveProvider = rasterizer.getGuideCurveProvider();
                context.guideCurveRegions = &chainStorage.curves.guideCurveRegions;
                context.offsetSeeds = nullptr;
                context.transferTable = Rasterization::TransferTable::values();

                chainUnsampleable = !Rasterization::WaveformBuildPolicy().build(
                        chainStorage.curves.curves,
                        context,
                        [this](int totalRes) {
                            updateChainBuffers(totalRes);
                            return Rasterization::WaveformBufferRefs(chainStorage.waveform.waveform);
                        });
            });
}

void VoiceMeshRasterizer::publishSnapshot() {
    Rasterization::RasterizerSnapshotSource source;

    if (chainedOutputActive) {
        source.intercepts = &chainStorage.intercepts.intercepts;
        source.colorPoints = &chainStorage.intercepts.colorPoints;
        source.curves = &chainStorage.curves.curves;
        source.waveform = chainStorage.waveform.waveform;
    } else {
        source = rasterizer.createSnapshotSource();
    }

    Rasterization::RasterizerSnapshotBuilder().publish(rasterizerData, source);
}

void VoiceMeshRasterizer::updateChainBuffers(int size) {
    chainStorage.waveform.waveform.place(chainWaveformMemory, size);
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

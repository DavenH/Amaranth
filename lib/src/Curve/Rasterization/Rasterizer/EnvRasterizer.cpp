#include "EnvRasterizer.h"

#include <Definitions.h>
#include <Curve/GuideCurveProvider.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include <Curve/Rasterization/Policies/Core/InterceptPolicies.h>
#include <Curve/Rasterization/Policies/Curves/CurvePolicies.h>
#include <Curve/Rasterization/Policies/Curves/WaveformBakePolicy.h>
#include <Curve/Rasterization/Policies/Envelope/EnvelopePolicies.h>
#include <Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h>
#include <Curve/Rasterization/Sampling/GuideCurveSampler.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>

#include <Util/Arithmetic.h>
#include <Util/CommonEnums.h>

namespace {
    Rasterization::EnvelopePaddingContext::State toEnvelopePaddingState(int state) {
        if (state == EnvRasterizer::Looping) {
            return Rasterization::EnvelopePaddingContext::Looping;
        }

        if (state == EnvRasterizer::Releasing) {
            return Rasterization::EnvelopePaddingContext::Releasing;
        }

        return Rasterization::EnvelopePaddingContext::NormalState;
    }

    String describeEnvelopeMesh(const EnvelopeMesh* mesh) {
        if (mesh == nullptr) {
            return "mesh=null";
        }

        return String::formatted("mesh=%p name=%s verts=%d cubes=%d",
                                 mesh,
                                 mesh->getName().toRawUTF8(),
                                 mesh->getNumVerts(),
                                 mesh->getNumCubes());
    }

    void copyRenderResult(
            const Rasterization::RenderResult& source,
            Rasterization::RenderResult& destination) {
        destination.intercepts = source.intercepts;
        destination.frontPadding = source.frontPadding;
        destination.backPadding = source.backPadding;
        destination.curves = source.curves;
        destination.guideCurveRegions = source.guideCurveRegions;
        destination.colorPoints = source.colorPoints;
        destination.paddingSize = source.paddingSize;
        destination.sampleable = source.sampleable;
        destination.needsResorting = source.needsResorting;

        const int waveformSize = source.waveform.waveX.size();
        destination.waveform.place(destination.waveformMemory, waveformSize);
        destination.waveform.copyFrom(source.waveform);
    }
}

EnvRasterizer::EnvRasterizer(GuideCurveProvider* guideCurveProvider, const String& nameToUse) :
         loopIndex       (-1)
    ,    sustainIndex    (-1)
    ,    envMesh         (nullptr)
    ,    guideCurveProvider(nullptr)
    ,    request()
    ,    result()
    ,    guideCurveOffsetSeeds()
    ,    reduction()
    ,    name(nameToUse) {
    setWrapsEnds(false);
    setLimits(0.f, 10.f);
    result.paddingSize = 2;
    rasterizerData.paddingSize = result.paddingSize;
    rasterizerData.wrapsVertices = request.cyclic;
    result.waveform.place(result.waveformMemory, 2048);
    loopResult.waveform.place(loopResult.waveformMemory, 2048);

    setGuideCurveProvider(guideCurveProvider);
}

EnvRasterizer& EnvRasterizer::operator=(const EnvRasterizer& copy) {
    this->envMesh               = copy.envMesh;
    this->loopIndex             = copy.loopIndex;
    this->sustainIndex          = copy.sustainIndex;
    this->playback              = copy.playback;
    this->guideCurveProvider    = copy.guideCurveProvider;
    this->request               = copy.request;
    this->name                  = copy.name;
    this->rasterizerData.paddingSize = copy.rasterizerData.paddingSize;
    this->rasterizerData.wrapsVertices = copy.rasterizerData.wrapsVertices;
    this->result.sampleable     = false;
    this->result.needsResorting = false;

    result.waveform.place(result.waveformMemory, 2048);
    loopResult.waveform.place(loopResult.waveformMemory, 2048);
    return *this;
}

EnvRasterizer::EnvRasterizer(const EnvRasterizer& copy) {
    operator=(copy);
}

EnvRasterizer::~EnvRasterizer() = default;

void EnvRasterizer::adoptPreparedData(const EnvRasterizer& source) {
    envMesh = source.envMesh;
    request = source.request;
    rasterizerData.paddingSize = source.rasterizerData.paddingSize;
    rasterizerData.wrapsVertices = source.rasterizerData.wrapsVertices;
    loopIndex = source.loopIndex;
    sustainIndex = source.sustainIndex;

    copyRenderResult(source.result, result);
    copyRenderResult(source.loopResult, loopResult);
    validateState();
}

void EnvRasterizer::setMesh(EnvelopeMesh* envelopeMesh) {
    envMesh = envelopeMesh;

    // DBG(String::formatted("EnvRasterizer[%s] setMesh(EnvelopeMesh*) %s",
    //                       getName().toRawUTF8(),
    //                       describeEnvelopeMesh(envMesh).toRawUTF8()));
}

void EnvRasterizer::setMesh(Mesh* mesh) {
    envMesh = dynamic_cast<EnvelopeMesh*>(mesh);
    jassert(mesh == nullptr || envMesh != nullptr);
}

void EnvRasterizer::installEnvelopeProviders() {
}

bool EnvRasterizer::hasReleaseCurve() {
    return Rasterization::EnvelopePlaybackPolicy().hasReleaseCurve(result.intercepts, sustainIndex);
}

void EnvRasterizer::renderEnvelopeCrossPoints() {
    getMorphPosition().resetTime();
    renderWaveformOnly(envMesh, 0.f);

    publishCurrentResult();

    //    evaluateLoopSustainIndices();
}

void EnvRasterizer::getIndices(int& loopIdx, int& sustIdx) const {
    loopIdx = loopIndex;
    sustIdx = sustainIndex;
}

void EnvRasterizer::setWantOneSamplePerCycle(bool does) {
    playback.setOneSamplePerCycle(does);
    setDecoupleComponentDfrm(does);
}

bool EnvRasterizer::canLoop() const {
    return loopIndex >= 0; // && sustainIndex - loopIndex >= loopMinSizeIcpts;
}

const Rasterization::RenderResult& EnvRasterizer::samplingResult() const {
    return getMode() == Looping && loopResult.sampleable ? loopResult : result;
}

float EnvRasterizer::getLoopLength() const {
    Rasterization::EnvelopePlaybackContext context;
    context.loopIndex = loopIndex;
    context.sustainIndex = sustainIndex;

    return Rasterization::EnvelopePlaybackPolicy().loopLength(result.intercepts, context);
}

Rasterization::EnvelopePaddingContext EnvRasterizer::createPaddingContext() const {
    Rasterization::EnvelopePaddingContext context;
    context.state             = toEnvelopePaddingState(getMode());
    context.loopMinSizeIcpts  = loopMinSizeIcpts;
    context.loopIndex         = loopIndex;
    context.sustainIndex      = sustainIndex;
    context.loopLength        = getLoopLength();
    context.canLoop           = canLoop();
    context.hasReleaseCurve   = Rasterization::EnvelopePlaybackPolicy().hasReleaseCurve(result.intercepts, sustainIndex);

    return context;
}

void EnvRasterizer::setNoteOn() {
    dbg("\tnote on for " << name);
    playback.noteOn();
}

void EnvRasterizer::setNoteOff() {
    playback.noteOff(preparedPlaybackView());
}

void EnvRasterizer::resetGraphicParams() {
    playback.resetGraphicVoice();
}

Mesh* EnvRasterizer::getCurrentMesh() {
    return envMesh;
}

void EnvRasterizer::updateWaveform(Mesh* mesh, float oscPhase) {
    renderWaveformOnly(mesh, oscPhase);
    publishCurrentResult();
}

void EnvRasterizer::renderWaveformOnly(Mesh* mesh, float oscPhase) {
    renderEnvelope(mesh, oscPhase, true);
}

void EnvRasterizer::renderEnvelope(Mesh* mesh, float oscPhase, bool buildWaveform) {
    EnvelopeMesh* envelopeMesh = dynamic_cast<EnvelopeMesh*>(mesh);
    jassert(mesh == nullptr || envelopeMesh != nullptr);

    if (mesh == nullptr || mesh->getNumCubes() == 0 || envelopeMesh == nullptr) {
        clearOutput();
        return;
    }

    envMesh = envelopeMesh;

    Rasterization::GuideCurveApplier guideApplier = createGuideCurveApplier();
    Rasterization::TrilinearMeshSlicer().sliceMesh(
            mesh,
            request,
            oscPhase,
            guideApplier,
            result,
            reduction);

    processEnvelopeIntercepts(result.intercepts);
    Rasterization::InterceptSortPolicy(&result.needsResorting).sortIfNeeded(result.intercepts);

    switch (Rasterization::InterceptDegeneracyPolicy().classify(result.intercepts.size())) {
        case Rasterization::InterceptDegeneracyAction::CleanUp:
            clearOutput();
            return;
        case Rasterization::InterceptDegeneracyAction::MarkUnsampleable:
            result.curves.clear();
            markWaveformUnsampleable();
            return;
        case Rasterization::InterceptDegeneracyAction::Continue:
            break;
    }

    Rasterization::InterceptPaddingFlagPolicy().apply(result.intercepts);

    if (buildWaveform) {
        rebuildCurvesFromIntercepts();
        preparePlaybackResults();
    }

}

void EnvRasterizer::calcIntercepts() {
    updateGeometry();
}

void EnvRasterizer::cleanUp() {
    clearOutput();
    publishCurrentResult();
}

void EnvRasterizer::clearOutput() {
    clearRasterizationResult(true);
    result.guideCurveRegions.clear();
    loopResult.clear();
}

void EnvRasterizer::updateGeometry() {
    renderGeometryOnly(envMesh, 0.f);
    publishCurrentResult();
}

void EnvRasterizer::updateGeometry(Mesh* mesh, float oscPhase) {
    renderGeometryOnly(mesh, oscPhase);
    publishCurrentResult();
}

void EnvRasterizer::renderGeometryOnly(Mesh* mesh, float oscPhase) {
    renderEnvelope(mesh, oscPhase, false);
}

void EnvRasterizer::updateWaveform() {
    renderEnvelopeCrossPoints();
}

bool EnvRasterizer::canRasterizeWaveform() {
    return envMesh != nullptr && envMesh->hasEnoughCubesForCrossSection();
}

bool EnvRasterizer::renderToBuffer(
    const int numSamples,
    const double deltaX,
    int paramIndex,
    const MeshLibrary::EnvProps& props,
    float tempoScale) {
    return playback.renderToBuffer(
            preparedPlaybackView(),
            numSamples,
            deltaX,
            paramIndex,
            props,
            tempoScale);
}

void EnvRasterizer::simulateStart(double& lastPosition) {
    playback.simulateStart(lastPosition);
}

bool EnvRasterizer::simulateStop(double& lastPosition) {
    return playback.simulateStop(preparedPlaybackView(), lastPosition);
}

bool EnvRasterizer::simulateRender(
    double advancement,
    double& lastPosition,
    const MeshLibrary::EnvProps& props,
    float tempoScale) {
    return playback.simulateRender(
            preparedPlaybackView(),
            advancement,
            lastPosition,
            props,
            tempoScale);
}

void EnvRasterizer::evaluateLoopSustainIndices() {
    auto markers = Rasterization::EnvelopeMarkerPolicy().evaluate(result.intercepts, envMesh, loopMinSizeIcpts);
    loopIndex = markers.loopIndex;
    sustainIndex = markers.sustainIndex;
}

void EnvRasterizer::processEnvelopeIntercepts(vector<Intercept>& intercepts) {
    evaluateLoopSustainIndices();

    Rasterization::EnvelopeSustainPointContext context;
    context.sustainIndex = sustainIndex;
    context.addFloorPoint = getScalingType() != Rasterization::PointScalingMode::Bipolar;

    result.needsResorting |= Rasterization::EnvelopeSustainPointPolicy().apply(intercepts, context);
}

void EnvRasterizer::rebuildCurvesFromIntercepts() {
    result.curves.clear();

    Rasterization::EnvelopePaddingContext context = createPaddingContext();

    if (!Rasterization::EnvelopePaddingPolicy().buildDisplayPadding(
            result.intercepts,
            result.curves,
            context)) {
        markWaveformUnsampleable();
        return;
    }

    bakeWaveform(result);
}

void EnvRasterizer::preparePlaybackResults() {
    loopResult.clear();
    if (!canLoop()) {
        return;
    }

    loopResult.intercepts = result.intercepts;
    loopResult.paddingSize = result.paddingSize;
    Rasterization::EnvelopePaddingContext context = createPaddingContext();
    context.state = Rasterization::EnvelopePaddingContext::Looping;
    if (!Rasterization::EnvelopePaddingPolicy().buildRenderPadding(
            loopResult.intercepts,
            loopResult.curves,
            context)) {
        return;
    }

    bakeWaveform(loopResult);
}

void EnvRasterizer::bakeWaveform(Rasterization::RenderResult& target) {
    if (target.curves.size() < 2) {
        return;
    }

    Rasterization::CurveResolutionPolicy::Context resolutionContext;
    resolutionContext.lowResCurves = request.lowResCurves;
    resolutionContext.integralSampling = request.integralSampling;
    resolutionContext.interpolateCurves = request.interpolateCurves;
    resolutionContext.paddingSize = target.paddingSize;
    Rasterization::CurveResolutionPolicy().apply(target.curves, resolutionContext);

    Rasterization::CurveWaveformPreparationPolicy().apply(target.curves);

    if (request.decoupleComponentDeforms) {
        target.guideCurveRegions.clear();
    }

    Rasterization::WaveformBakePolicy::Context context;
    context.lowResCurves = request.lowResCurves;
    context.decoupleComponentDfrms = request.decoupleComponentDeforms;
    context.noiseSeed = request.noiseSeed;
    context.morph = request.morph;
    context.guideCurveProvider = guideCurveProvider;
    context.guideCurveRegions = &target.guideCurveRegions;
    context.offsetSeeds = &guideCurveOffsetSeeds;

    target.sampleable = Rasterization::WaveformBakePolicy().build(
            target.curves,
            context,
            [&target](int totalRes) {
                target.waveform.place(target.waveformMemory, totalRes);
                return Rasterization::WaveformBufferRefs(target.waveform);
            });
}

void EnvRasterizer::publishCurrentResult() {
    Rasterization::RasterizerSnapshotSource source;
    source.intercepts = &result.intercepts;
    source.colorPoints = &result.colorPoints;
    source.curves = &result.curves;
    source.waveform = result.waveform;
    source.paddingSize = result.paddingSize;
    source.wrapsVertices = request.cyclic;
    source.sampleable = result.sampleable;

    Rasterization::BaseRasterizer::publishSnapshot(source);
}

void EnvRasterizer::markWaveformUnsampleable() {
    result.waveform.waveX.nullify();
    result.waveform.waveY.nullify();
    result.sampleable = false;
}

void EnvRasterizer::clearRasterizationResult(bool clearCurves) {
    markWaveformUnsampleable();

    result.intercepts.clear();
    result.frontPadding.clear();
    result.backPadding.clear();
    result.colorPoints.clear();

    if (clearCurves) {
        result.curves.clear();
    }
}

Rasterization::GuideCurveApplier EnvRasterizer::createGuideCurveApplier() {
    Rasterization::GuideCurvePolicyContext context;
    context.guideCurveProvider = guideCurveProvider;
    context.reduction = &reduction;
    context.scalingMode = request.scalingMode;
    context.cyclic = request.cyclic;
    context.needsResorting = &result.needsResorting;
    context.noiseSeed = request.noiseSeed;
    context.offsetSeeds = &guideCurveOffsetSeeds;

    return Rasterization::GuideCurveApplier(context);
}

void EnvRasterizer::validateState() {
    playback.validate(preparedPlaybackView());
}

void EnvRasterizer::ensureParamSize(int numUnisonVoices) {
    playback.ensureVoiceCount(numUnisonVoices);
}

void EnvRasterizer::updateOffsetSeeds(int layerSize, int tableSize) {
    if (playback.oneSamplePerCycle()) {
        playback.randomizeVoiceOffsets(tableSize);
        return;
    }

    Random rand(Time::currentTimeMillis());
    guideCurveOffsetSeeds.randomize(layerSize, tableSize, rand);
}

Rasterization::PreparedEnvelopePlaybackView EnvRasterizer::preparedPlaybackView() const {
    return {
            result,
            loopResult,
            loopIndex,
            sustainIndex,
            request.scalingMode,
            guideCurveProvider,
            request.noiseSeed
    };
}

void EnvRasterizer::updateValue(int dim, float value) {
    switch (dim) {
        case CommonEnums::YellowDim: request.morph.time = value; break;
        case CommonEnums::RedDim:    request.morph.red = value;  break;
        case CommonEnums::BlueDim:   request.morph.blue = value; break;
        default: break;
    }
}

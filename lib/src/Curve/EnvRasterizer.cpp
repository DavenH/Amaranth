#include "EnvRasterizer.h"

#include <Definitions.h>

#include "EnvelopeMesh.h"
#include "GuideCurveProvider.h"
#include "Rasterization/Builders/TransferTable.h"
#include "Rasterization/Interpolation/TrilinearMeshSlicer.h"
#include "Rasterization/Pipelines/MeshSlicePipeline.h"
#include "Rasterization/Policies/Core/InterceptPolicies.h"
#include "Rasterization/Policies/Curves/CurveResolutionPolicy.h"
#include "Rasterization/Policies/Curves/CurveWaveformPreparationPolicy.h"
#include "Rasterization/Policies/Curves/InterceptPaddingFlagPolicy.h"
#include "Rasterization/Policies/Curves/WaveformBakePolicy.h"
#include "Rasterization/Policies/Curves/WaveformBuildPolicy.h"
#include "Rasterization/Policies/Envelope/EnvelopePlaybackPolicy.h"
#include "Rasterization/Policies/Envelope/EnvelopeMarkerPolicy.h"
#include "Rasterization/Policies/Envelope/EnvelopePaddingPolicy.h"
#include "Rasterization/Policies/Envelope/EnvelopeRenderTimingPolicy.h"
#include "Rasterization/Policies/Envelope/EnvelopeReleasePolicy.h"
#include "Rasterization/Policies/Envelope/EnvelopeStateValidationPolicy.h"
#include "Rasterization/Policies/Envelope/EnvelopeSustainPointPolicy.h"
#include "Rasterization/Policies/Mesh/GuideCurvePolicy.h"
#include "Rasterization/Sampling/GuideCurveSampler.h"
#include "Rasterization/Sampling/WaveformSampler.h"
#include "Rasterization/Sources/MeshCubeSource.h"
#include "../App/SingletonRepo.h"
#include "../Util/Arithmetic.h"
#include "../Util/CommonEnums.h"

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
}

EnvRasterizer::EnvRasterizer(SingletonRepo* repo, GuideCurveProvider* guideCurveProvider, const String& name) :
        SingletonAccessor(repo, name)
    ,    sampleReleaseNextCall(false)
    ,    oneSamplePerCycle(false)
    ,    loopIndex       (-1)
    ,    sustainIndex    (-1)
    ,    state           (NormalState)
    ,    releaseScale    (1.f)
    ,    envMesh         (nullptr)
    ,    preallocated    (8192)
    ,    waveformMemory  (4096)
    ,    guideCurveProvider(nullptr)
    ,    request()
    ,    result()
    ,    guideCurveOffsetSeeds()
    ,    rasterizerData()
    ,    reduction()
    ,    paddingSize(2)
    ,    unsampleable(true)
    ,    needsResorting(false) {
    setWrapsEnds(false);
    setLimits(0.f, 10.f);
    updateBuffers(2048);

    // graphic params
    params.emplace_back();

    // head unison index
    params.emplace_back();

    setGuideCurveProvider(guideCurveProvider);
}

EnvRasterizer& EnvRasterizer::operator=(const EnvRasterizer& copy) {
    this->sampleReleaseNextCall = copy.sampleReleaseNextCall;
    this->envMesh               = copy.envMesh;
    this->oneSamplePerCycle     = copy.oneSamplePerCycle;
    this->loopIndex             = copy.loopIndex;
    this->sustainIndex          = copy.sustainIndex;
    this->state                 = copy.state;
    this->releaseScale          = copy.releaseScale;
    this->guideCurveProvider    = copy.guideCurveProvider;
    this->request               = copy.request;
    this->paddingSize           = copy.paddingSize;
    this->unsampleable          = true;
    this->needsResorting        = false;

    params = copy.params;

    preallocated.ensureSize(8192);
    updateBuffers(2048);
    return *this;
}

EnvRasterizer::EnvRasterizer(const EnvRasterizer& copy) :
        SingletonAccessor(copy) {
    operator=(copy);
}

EnvRasterizer::~EnvRasterizer() {
    preallocated.clear();
    waveformMemory.clear();
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
    calcCrossPoints(envMesh, 0.f);

    // do this even if we can't loop right now just in case
    // loopability changes by the time release curve is going
    // to be recalculated
    copyWaveformForRelease();

    //    evaluateLoopSustainIndices();
}

void EnvRasterizer::padIcptsForRender(vector<Intercept>& intercepts, vector<Curve>& curves) {
    Rasterization::EnvelopePaddingContext context = createPaddingContext();

    if (!Rasterization::EnvelopePaddingPolicy().buildRenderPadding(intercepts, curves, context)) {
        markWaveformUnsampleable();
    }
}

void EnvRasterizer::getIndices(int& loopIdx, int& sustIdx) const {
    loopIdx = loopIndex;
    sustIdx = sustainIndex;
}

void EnvRasterizer::setWantOneSamplePerCycle(bool does) {
    oneSamplePerCycle = does;
    setDecoupleComponentDfrm(does);
}

bool EnvRasterizer::canLoop() const {
    return loopIndex >= 0; // && sustainIndex - loopIndex >= loopMinSizeIcpts;
}

float EnvRasterizer::getLoopLength() const {
    Rasterization::EnvelopePlaybackContext context;
    context.loopIndex = loopIndex;
    context.sustainIndex = sustainIndex;

    return Rasterization::EnvelopePlaybackPolicy().loopLength(result.intercepts, context);
}

Rasterization::EnvelopePaddingContext EnvRasterizer::createPaddingContext() const {
    Rasterization::EnvelopePaddingContext context;
    context.state             = toEnvelopePaddingState(state);
    context.loopMinSizeIcpts  = loopMinSizeIcpts;
    context.loopIndex         = loopIndex;
    context.sustainIndex      = sustainIndex;
    context.loopLength        = getLoopLength();
    context.canLoop           = canLoop();
    context.hasReleaseCurve   = Rasterization::EnvelopePlaybackPolicy().hasReleaseCurve(result.intercepts, sustainIndex);

    return context;
}

void EnvRasterizer::setNoteOn() {
    state = NormalState;

    dbg("\tnote on for " << getName());

    sampleReleaseNextCall = false;

    for (int i = headUnisonIndex; i < (int) params.size(); ++i) {
        params[i].reset();
    }
}

void EnvRasterizer::setNoteOff() {
    if (state != Releasing) {
        if (hasReleaseCurve()) {
            state = Releasing;
            sampleReleaseNextCall = true;
        }
    }
}

void EnvRasterizer::resetGraphicParams() {
    params[graphicIndex].reset();
}

Mesh* EnvRasterizer::getCurrentMesh() {
    return envMesh;
}

void EnvRasterizer::calcCrossPoints() {
    renderEnvelopeCrossPoints();
}

void EnvRasterizer::calcCrossPoints(Mesh* mesh, float oscPhase) {
    EnvelopeMesh* envelopeMesh = dynamic_cast<EnvelopeMesh*>(mesh);
    jassert(mesh == nullptr || envelopeMesh != nullptr);

    if (mesh == nullptr || mesh->getNumCubes() == 0 || envelopeMesh == nullptr) {
        cleanUp();
        return;
    }

    envMesh = envelopeMesh;
    result.intercepts.clear();
    result.colorPoints.clear();
    result.curves.clear();
    result.guideCurveRegions.clear();
    needsResorting = false;

    Rasterization::GuideCurveApplier guideApplier = createGuideCurveApplier();

    Rasterization::MeshSlicePipeline meshSlicePipeline;
    const Rasterization::RenderResult& output = meshSlicePipeline.renderWithReduction(
            Rasterization::MeshCubeSource(mesh),
            Rasterization::TrilinearMeshSlicer(),
            request,
            oscPhase,
            guideApplier,
            reduction);
    result.intercepts = output.intercepts;

    if (request.calcDepthDimensions) {
        result.colorPoints = output.colorPoints;
    }

    processEnvelopeIntercepts(result.intercepts);
    Rasterization::InterceptSortPolicy(&needsResorting).sortIfNeeded(result.intercepts);

    switch (Rasterization::InterceptDegeneracyPolicy().classify(result.intercepts.size())) {
        case Rasterization::InterceptDegeneracyAction::CleanUp:
            cleanUp();
            return;
        case Rasterization::InterceptDegeneracyAction::MarkUnsampleable:
            result.curves.clear();
            markWaveformUnsampleable();
            publishSnapshot();
            return;
        case Rasterization::InterceptDegeneracyAction::Continue:
            break;
    }

    Rasterization::InterceptPaddingFlagPolicy().apply(result.intercepts);

    if (!request.calcInterceptsOnly) {
        rebuildCurvesFromIntercepts();
    }

    publishSnapshot();
}

void EnvRasterizer::calcIntercepts() {
    ScopedValueSetter calcInterceptsOnly(request.calcInterceptsOnly, true, request.calcInterceptsOnly);
    calcCrossPoints();
    publishSnapshot();
}

void EnvRasterizer::cleanUp() {
    clearRasterizationResult(false);
    result.guideCurveRegions.clear();
    publishSnapshot();
}

void EnvRasterizer::performUpdate(UpdateType updateType) {
    if (updateType == Update) {
        calcCrossPoints();
    }
}

bool EnvRasterizer::hasEnoughCubesForCrossSection() {
    return envMesh != nullptr && envMesh->hasEnoughCubesForCrossSection();
}

bool EnvRasterizer::isSampleable() {
    return Rasterization::WaveformSampler::isSampleable(result.waveform);
}

bool EnvRasterizer::isSampleableAt(float x) {
    return Rasterization::WaveformSampler::isSampleableAt(result.waveform, x);
}

float EnvRasterizer::sampleAt(double angle) {
    return Rasterization::WaveformSampler::sampleAt(result.waveform, unsampleable, angle);
}

float EnvRasterizer::sampleAt(double angle, int& currentIndex) {
    return Rasterization::WaveformSampler::sampleAt(result.waveform, unsampleable, angle, currentIndex);
}

float EnvRasterizer::sampleAtDecoupled(double angle, GuideCurveContext& context) {
    return Rasterization::GuideCurveSampler::sampleDecoupled(
            result.waveform,
            unsampleable,
            angle,
            context,
            result.guideCurveRegions,
            guideCurveProvider,
            request.noiseSeed);
}

float EnvRasterizer::samplePerfectly(double delta, Buffer<float> buffer, double phase) {
    return Rasterization::WaveformSampler::samplePerfectly(result.waveform, delta, buffer, phase);
}

bool EnvRasterizer::renderToBuffer(
    const int numSamples,
    const double deltaX,
    int paramIndex,
    const MeshLibrary::EnvProps& props,
    float tempoScale) {
    if (!props.active) {
        return false;
    }

    jassert(deltaX > 0);

    Rasterization::EnvelopeRenderTimingContext timingContext;
    timingContext.numSamples = numSamples;
    timingContext.deltaX = deltaX;
    timingContext.tempoScale = tempoScale;
    timingContext.loopLength = getLoopLength();
    timingContext.props = &props;

    auto timing = Rasterization::EnvelopeRenderTimingPolicy().prepare(timingContext);

    if (!oneSamplePerCycle) {
        // should happen extremely rarely�only when buffer sizes > 8192
        preallocated.ensureSize(numSamples);
        renderBuffer = preallocated.withSize(numSamples);
    }

    int bufferPos = 0;
    bool stillAlive = true;

    // partition buffers because state may change within buffer
    while (numSamples - bufferPos > 0) {
        int maxSamples = jmin(timing.maxSamplesPerBuffer, numSamples - bufferPos);
        int samplesRendered = maxSamples;

        Buffer<float> buffer;
        if (!oneSamplePerCycle) {
            buffer = Buffer(renderBuffer + bufferPos, maxSamples);
        }

        if (stillAlive) {
            samplesRendered = vectorizedRenderToBuffer(buffer, maxSamples, timing.effectiveDelta, paramIndex);

            stillAlive &= samplesRendered == maxSamples;
        }

        bufferPos += samplesRendered;

        if (!stillAlive) {
            dbg("no longer alive");

            if (oneSamplePerCycle) {
                break;
            }

            if (bufferPos < renderBuffer.size()) {
                renderBuffer.offset(bufferPos).zero();
            }
        }
    }

    if (props.logarithmic && !oneSamplePerCycle) {
        Arithmetic::applyInvLogMapping(renderBuffer, 30);
    }

    return stillAlive;
}

int EnvRasterizer::vectorizedRenderToBuffer(
    Buffer<float> buffer,
    const int numSamples,
    const double deltaX,
    int paramIndex) {
    auto& intercepts = result.intercepts;
    auto& curves = result.curves;
    auto waveform = result.waveform;
    EnvParams& group = params[paramIndex];

    dbg("\n\n" << getName() << "(" << paramIndex << ")");

    double advancement = numSamples * deltaX;
    Rasterization::EnvelopePlaybackContext context;
    context.loopIndex = loopIndex;
    context.sustainIndex = sustainIndex;
    context.releasing = state == Releasing;

    double boundary = Rasterization::EnvelopePlaybackPolicy().boundary(intercepts, context);
    double nextPosition = group.samplePosition + advancement;
    bool overextends = nextPosition > boundary;
    bool loopable = canLoop();
    bool willLoopBackNextCall = loopable && overextends && state == NormalState;
    int samplesRendered = numSamples;

    dbg("rendering to buffer\t" << buffer.size() << " with delta " << deltaX <<
        ", range " << group.samplePosition << " - " << nextPosition);

    if (willLoopBackNextCall) {
        dbg("calculating loop curves, state = looping");
        state = Looping;

        padIcptsForRender(intercepts, curves);
        bakeWaveform();
    }

    if (sampleReleaseNextCall) {
        sampleReleaseNextCall = false;
        changedToRelease();
    }

    switch (state) {
        case NormalState: {
            dbg("normal state");

            if (oneSamplePerCycle) {
                group.sustainLevel = sampleAtDecoupled(group.samplePosition, group.guideCurveContext);
            } else {
                int maxSamples = jmin(numSamples, int((boundary - group.samplePosition) / deltaX));

                if (maxSamples > 0) {
                    sampleWithInterval(buffer.withSize(maxSamples), deltaX, group.samplePosition);

                    group.sustainLevel = buffer[maxSamples - 1];
                    dbg("sampled " << maxSamples);
                }

                if (maxSamples < numSamples) {
                    buffer.offset(maxSamples).set(group.sustainLevel);
                    dbg("set " << (numSamples - maxSamples) << " samples to level " << group.sustainLevel);
                }
            }

            group.samplePosition = jmin(boundary, group.samplePosition + advancement);
            break;
        }

        case Looping: {
            double loopLength = getLoopLength();
            jassert(loopLength > 0);

            dbg("looping state, loop length " << loopLength);

            if (oneSamplePerCycle) {
                group.sustainLevel = sampleAtDecoupled(group.samplePosition, group.guideCurveContext);

                while (overextends) {
                    group.samplePosition -= loopLength;
                    overextends = group.samplePosition + advancement > boundary;
                }

                group.samplePosition = jmin(boundary, group.samplePosition + advancement);
            } else {
                jassert(numSamples > 0);

                group.sampleIndex = waveform.zeroIndex;

                for (int i = 0; i < numSamples; ++i) {
                    buffer[i] = sampleAt(group.samplePosition, group.sampleIndex);
                    group.samplePosition += deltaX;

                    if (group.samplePosition >= boundary) {
                        group.samplePosition -= loopLength - deltaX;
                    }
                }

                group.sustainLevel = buffer[numSamples - 1];
            }

            break;
        }

        case Releasing: {
            dbg("release mode");

            if (!hasReleaseCurve()) {
                dbg("no release mesh... returning");

                return 0;
            }

            if (oneSamplePerCycle) {
                jassert(isSampleableAt(boundary));

                if (group.samplePosition <= boundary) {
                    group.sustainLevel = releaseScale * sampleAtDecoupled(group.samplePosition, group.guideCurveContext);
                }
            } else {
                int maxSamples = jmin(numSamples, int((boundary - group.samplePosition) / deltaX));

                // voice should have been stopped before violating this
                jassert(group.samplePosition <= boundary);

                Buffer<float> rendBuf(buffer, maxSamples);
                sampleWithInterval(rendBuf, deltaX, group.samplePosition);

                rendBuf.mul(releaseScale);

                dbg("sampled " << maxSamples << " samples");

                if (maxSamples < numSamples) {
                    buffer.offset(maxSamples).zero();
                }

                samplesRendered = maxSamples;
            }

            group.samplePosition = jmin(boundary, group.samplePosition + advancement);

            break;
        }
        default:
            throw std::invalid_argument("invalid state");
    }

    return samplesRendered;
}

void EnvRasterizer::simulateStart(double& lastPosition) {
    sampleReleaseNextCall = false;
    lastPosition = 0;
    state = NormalState;
}

bool EnvRasterizer::simulateStop(double& lastPosition) {
    setNoteOff();

    if (hasReleaseCurve()) {
        lastPosition = result.intercepts[sustainIndex].x;
        return true;
    }

    return false;
}

bool EnvRasterizer::simulateRender(
    double advancement,
    double& lastPosition,
    const MeshLibrary::EnvProps& props,
    float tempoScale) {
    auto& intercepts = result.intercepts;

    jassert(! intercepts.empty());
    jassert(state == Releasing || isPositiveAndBelow(sustainIndex, (int) intercepts.size()));

    if (intercepts.empty() || (state != Releasing && ! isPositiveAndBelow(sustainIndex, (int) intercepts.size()))) {
        DBG(String::formatted("EnvRasterizer[%s] simulateRender invalid state mesh=%p icpts=%d sustain=%d state=%d",
                              getName().toRawUTF8(),
                              envMesh,
                              (int) intercepts.size(),
                              sustainIndex,
                              state));
        return false;
    }

    Rasterization::EnvelopeRenderTimingContext timingContext;
    timingContext.deltaX = advancement;
    timingContext.tempoScale = tempoScale;
    timingContext.loopLength = getLoopLength();
    timingContext.props = &props;

    advancement = Rasterization::EnvelopeRenderTimingPolicy().prepare(timingContext).effectiveDelta;

    Rasterization::EnvelopePlaybackContext context;
    context.loopIndex = loopIndex;
    context.sustainIndex = sustainIndex;
    context.releasing = state == Releasing;

    double boundary = Rasterization::EnvelopePlaybackPolicy().boundary(intercepts, context);
    double nextPosition = lastPosition + advancement;
    bool overextends = nextPosition > boundary;
    bool loopable = canLoop();
    bool willLoopBackNextCall = loopable && overextends && state == NormalState;

    if (willLoopBackNextCall) {
        state = Looping;
    }

    if (sampleReleaseNextCall) {
        sampleReleaseNextCall = false;

        Rasterization::EnvelopeReleaseContext releaseContext;
        releaseContext.bipolar = getScalingType() == Rasterization::PointScalingMode::Bipolar;
        releaseContext.sustainIndex = sustainIndex;

        int releaseIdx = Rasterization::EnvelopeReleasePolicy().releaseIndex(releaseContext);
        lastPosition = intercepts[releaseIdx].x;
    }

    switch (state) {
        case NormalState: {
            lastPosition = jmin(boundary, lastPosition + advancement);
            break;
        }

        case Looping: {
            double loopLength = getLoopLength();

            if (oneSamplePerCycle) {
                while (overextends) {
                    lastPosition -= loopLength;
                    overextends = lastPosition + advancement > boundary;
                }

                lastPosition = jmin(boundary, lastPosition + advancement);
            } else {
                lastPosition += advancement;

                while (lastPosition >= boundary) {
                    lastPosition -= loopLength;
                }
            }

            break;
        }

        case Releasing: {
            if (!hasReleaseCurve()) {
                return false;
            }

            lastPosition = jmin(boundary, lastPosition + advancement);

            if (lastPosition == boundary) {
                return false;
            }

            break;
        }
        default:
            throw std::runtime_error("EnvRasterizer::simulateRender: invalid state");
    }

    return true;
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

    needsResorting |= Rasterization::EnvelopeSustainPointPolicy().apply(intercepts, context);
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

    bakeWaveform();
}

void EnvRasterizer::bakeWaveform() {
    if (result.curves.size() < 2) {
        return;
    }

    Rasterization::CurveResolutionPolicy::Context resolutionContext;
    resolutionContext.lowResCurves = request.lowResCurves;
    resolutionContext.integralSampling = request.integralSampling;
    resolutionContext.interpolateCurves = request.interpolateCurves;
    resolutionContext.paddingSize = paddingSize;
    Rasterization::CurveResolutionPolicy().apply(result.curves, resolutionContext);

    Rasterization::CurveWaveformPreparationPolicy().apply(result.curves);

    if (request.decoupleComponentDeforms) {
        result.guideCurveRegions.clear();
    }

    Rasterization::WaveformBakePolicy::Context context;
    context.lowResCurves = request.lowResCurves;
    context.decoupleComponentDfrms = request.decoupleComponentDeforms;
    context.noiseSeed = request.noiseSeed;
    context.morph = request.morph;
    context.guideCurveProvider = guideCurveProvider;
    context.guideCurveRegions = &result.guideCurveRegions;
    context.offsetSeeds = &guideCurveOffsetSeeds;
    context.transferTable = Rasterization::TransferTable::values();

    unsampleable = !Rasterization::WaveformBuildPolicy().build(
            result.curves,
            context,
            [this](int totalRes) {
                updateBuffers(totalRes);
                return Rasterization::WaveformBufferRefs(result.waveform);
            });
}

void EnvRasterizer::copyWaveformForRelease() {
    auto waveform = result.waveform;
    int size = waveform.waveX.size();

    if (size > 0) {
        waveformMemory.ensureSize(3 * size);

        waveXCopy = waveformMemory.place(size);
        waveYCopy = waveformMemory.place(size);
        slopeCopy = waveformMemory.place(size);

        waveform.waveX.copyTo(waveXCopy);
        waveform.waveY.copyTo(waveYCopy);
        waveform.slope.copyTo(slopeCopy);
    } else {
        waveXCopy.nullify();
        waveYCopy.nullify();
        slopeCopy.nullify();
    }
}

void EnvRasterizer::publishSnapshot() {
    Rasterization::RasterizerSnapshotSource source;
    source.intercepts = &result.intercepts;
    source.colorPoints = &result.colorPoints;
    source.curves = &result.curves;
    source.waveform = result.waveform;

    Rasterization::RasterizerSnapshotBuilder().publish(rasterizerData, source);
}

void EnvRasterizer::updateBuffers(int size) {
    result.waveform.place(result.waveformMemory, size);
}

void EnvRasterizer::markWaveformUnsampleable() {
    result.waveform.waveX.nullify();
    result.waveform.waveY.nullify();
    unsampleable = true;
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
    context.needsResorting = &needsResorting;
    context.noiseSeed = request.noiseSeed;
    context.offsetSeeds = &guideCurveOffsetSeeds;

    return Rasterization::GuideCurveApplier(context);
}

void EnvRasterizer::changedToRelease() {
    jassert(state == Releasing);

    dbg("recalculating release");

    auto& intercepts = result.intercepts;

    if (canLoop()) {
        result.waveform.waveX = waveXCopy;
        result.waveform.waveY = waveYCopy;
        result.waveform.slope = slopeCopy;
    }

    float lastLevel = params[headUnisonIndex].sustainLevel;

    Rasterization::EnvelopeReleaseContext releaseContext;
    releaseContext.bipolar = getScalingType() == Rasterization::PointScalingMode::Bipolar;
    releaseContext.sustainIndex = sustainIndex;

    int releaseIdx = Rasterization::EnvelopeReleasePolicy().releaseIndex(releaseContext);
    float sampledReleaseLevel = sampleAt(intercepts[releaseIdx].x);
    auto release = Rasterization::EnvelopeReleasePolicy().start(
            intercepts,
            releaseContext,
            lastLevel,
            sampledReleaseLevel);

    releaseScale = release.scale;

    dbg("release scale: " << releaseScale);

    for (int i = headUnisonIndex; i < (int) params.size(); ++i) {
        params[i].samplePosition = release.position;
    }
}

void EnvRasterizer::validateState() {
    auto& intercepts = result.intercepts;
    auto waveform = result.waveform;

    Rasterization::EnvelopeStateValidationContext context;
    context.state = state == Looping
            ? Rasterization::EnvelopeStateValidationContext::Looping
            : state == Releasing
                    ? Rasterization::EnvelopeStateValidationContext::Releasing
                    : Rasterization::EnvelopeStateValidationContext::NormalState;
    context.headIndex = headUnisonIndex;
    context.waveformSize = waveform.waveX.size();
    context.waveformEnd = waveform.waveX.empty() ? 0. : waveform.waveX.back();
    context.loopLength = getLoopLength();
    context.loopStart = loopIndex >= 0 && loopIndex < (int) intercepts.size() ? intercepts[loopIndex].x : 0.;
    context.loopEnd = sustainIndex >= 0 && sustainIndex < (int) intercepts.size() ? intercepts[sustainIndex].x : 0.;

    auto result = Rasterization::EnvelopeStateValidationPolicy().validate(params, context);

    state = result == Rasterization::EnvelopeStateValidationContext::Looping
            ? Looping
            : result == Rasterization::EnvelopeStateValidationContext::Releasing
                    ? Releasing
                    : NormalState;
}

void EnvRasterizer::ensureParamSize(int numUnisonVoices) {
    if (params.size() == numUnisonVoices + 1) {
        return;
    }

    while (params.size() < numUnisonVoices + 1) {
        params.emplace_back();
    }
}

void EnvRasterizer::updateOffsetSeeds(int layerSize, int tableSize) {
    if (oneSamplePerCycle) {
        Random rand(Time::currentTimeMillis());

        for (auto& param: params) {
            GuideCurveContext& context = param.guideCurveContext;
            context.phaseOffsetSeed = rand.nextInt(tableSize);
            context.vertOffsetSeed = rand.nextInt(tableSize);
        }

        return;
    }

    Random rand(Time::currentTimeMillis());
    guideCurveOffsetSeeds.randomize(layerSize, tableSize, rand);
}

void EnvRasterizer::updateValue(int dim, float value) {
    switch (dim) {
        case CommonEnums::YellowDim: request.morph.time = value; break;
        case CommonEnums::RedDim:    request.morph.red = value;  break;
        case CommonEnums::BlueDim:   request.morph.blue = value; break;
        default: break;
    }
}

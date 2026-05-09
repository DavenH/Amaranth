#include "EnvRasterizer.h"

#include <Definitions.h>

#include "EnvelopeMesh.h"
#include "GuideCurveProvider.h"
#include "Rasterization/Facades/EnvRasterizerFacade.h"
#include "Rasterization/Policies/EnvelopePlaybackPolicy.h"
#include "Rasterization/Policies/EnvelopeRenderTimingPolicy.h"
#include "Rasterization/Policies/EnvelopeReleasePolicy.h"
#include "Rasterization/Policies/EnvelopeStateValidationPolicy.h"
#include "../App/SingletonRepo.h"
#include "../Util/Arithmetic.h"

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
    ,   MeshRasterizer   (name)
    ,    envMesh         (nullptr)
    ,    sampleReleaseNextCall(false)
    ,    oneSamplePerCycle(false)
    ,    state           (NormalState)
    ,    loopIndex       (-1)
    ,    sustainIndex    (-1)
    ,    releaseScale    (1.f)
    ,    preallocated    (8192)
    ,    waveformMemory  (4096) {
    setWrapsEnds(false);
    setLimits(0.f, 10.f);

    // graphic params
    params.emplace_back();

    // head unison index
    params.emplace_back();

    setGuideCurveProvider(guideCurveProvider);
    installEnvelopeProviders();
}

EnvRasterizer& EnvRasterizer::operator=(const EnvRasterizer& copy) {
    MeshRasterizer::operator =(copy);
    installEnvelopeProviders();

    this->sampleReleaseNextCall = copy.sampleReleaseNextCall;
    this->envMesh               = copy.envMesh;
    this->oneSamplePerCycle     = copy.oneSamplePerCycle;
    this->loopIndex             = copy.loopIndex;
    this->sustainIndex          = copy.sustainIndex;
    this->state                 = copy.state;
    this->releaseScale          = copy.releaseScale;

    params = copy.params;

    preallocated.ensureSize(8192);
    return *this;
}

EnvRasterizer::EnvRasterizer(const EnvRasterizer& copy) : SingletonAccessor(copy), MeshRasterizer(copy) {
    initialise();
    operator=(copy);
}

EnvRasterizer::~EnvRasterizer() {
    preallocated.clear();
    waveformMemory.clear();
}

void EnvRasterizer::setMesh(EnvelopeMesh* envelopeMesh) {
    MeshRasterizer::setMesh(envelopeMesh);

    envMesh = envelopeMesh;

    // DBG(String::formatted("EnvRasterizer[%s] setMesh(EnvelopeMesh*) %s",
    //                       MeshRasterizer::name.toRawUTF8(),
    //                       describeEnvelopeMesh(envMesh).toRawUTF8()));
}

void EnvRasterizer::installEnvelopeProviders() {
    setMeshAssignmentProvider([this](Mesh* assignedMesh) {
        envMesh = dynamic_cast<EnvelopeMesh*>(assignedMesh);
        jassert(assignedMesh == nullptr || envMesh != nullptr);
    });
}

bool EnvRasterizer::hasReleaseCurve() {
    return Rasterization::EnvelopePlaybackPolicy().hasReleaseCurve(icpts, sustainIndex);
}

void EnvRasterizer::calcCrossPoints() {
    getMorphPosition().resetTime();
    MeshRasterizer::calcCrossPoints(envMesh, 0.f);

    // do this even if we can't loop right now just in case
    // loopability changes by the time release curve is going
    // to be recalculated
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

    //    evaluateLoopSustainIndices();
}

void EnvRasterizer::processIntercepts(vector<Intercept>& intercepts) {
    evaluateLoopSustainIndices();

    Rasterization::EnvelopeSustainPointContext context;
    context.sustainIndex = sustainIndex;
    context.addFloorPoint = getScalingType() != Bipolar;

    needsResorting |= Rasterization::EnvRasterizerFacade().applySustainPoint(intercepts, context);
}

void EnvRasterizer::padIcptsForRender(vector<Intercept>& intercepts, vector<Curve>& curves) {
    Rasterization::EnvelopePaddingContext context = createPaddingContext();

    if (!Rasterization::EnvRasterizerFacade().buildRenderPadding(intercepts, curves, context)) {
        markWaveformUnsampleable();
    }
}

void EnvRasterizer::padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) {
    Rasterization::EnvelopePaddingContext context = createPaddingContext();

    if (!Rasterization::EnvRasterizerFacade().buildDisplayPadding(icpts, curves, context)) {
        markWaveformUnsampleable();
    }
}

void EnvRasterizer::getIndices(int& loopIdx, int& sustIdx) const {
    loopIdx = loopIndex;
    sustIdx = sustainIndex;
}

void EnvRasterizer::updateOffsetSeeds(int layerSize, int tableSize) {
    if (oneSamplePerCycle) {
        Random rand(Time::currentTimeMillis());

        for (auto& param: params) {
            GuideCurveContext& context = param.guideCurveContext;
            context.phaseOffsetSeed = rand.nextInt(tableSize);
            context.vertOffsetSeed = rand.nextInt(tableSize);
        }
    } else {
        MeshRasterizer::updateOffsetSeeds(layerSize, tableSize);
    }
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

    return Rasterization::EnvelopePlaybackPolicy().loopLength(icpts, context);
}

Rasterization::EnvelopePaddingContext EnvRasterizer::createPaddingContext() const {
    Rasterization::EnvelopePaddingContext context;
    context.state             = toEnvelopePaddingState(state);
    context.loopMinSizeIcpts  = loopMinSizeIcpts;
    context.loopIndex         = loopIndex;
    context.sustainIndex      = sustainIndex;
    context.loopLength        = getLoopLength();
    context.canLoop           = canLoop();
    context.hasReleaseCurve   = Rasterization::EnvelopePlaybackPolicy().hasReleaseCurve(icpts, sustainIndex);

    return context;
}

void EnvRasterizer::setNoteOn() {
    state = NormalState;

    dbg("\tnote on for " << MeshRasterizer::getName());

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
    EnvParams& group = params[paramIndex];

    dbg("\n\n" << MeshRasterizer::getName() << "(" << paramIndex << ")");

    double advancement = numSamples * deltaX;
    Rasterization::EnvelopePlaybackContext context;
    context.loopIndex = loopIndex;
    context.sustainIndex = sustainIndex;
    context.releasing = state == Releasing;

    double boundary = Rasterization::EnvelopePlaybackPolicy().boundary(icpts, context);
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

        padIcptsForRender(icpts, curves);
        updateCurves();
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
        lastPosition = icpts[sustainIndex].x;
        return true;
    }

    return false;
}

bool EnvRasterizer::simulateRender(
    double advancement,
    double& lastPosition,
    const MeshLibrary::EnvProps& props,
    float tempoScale) {
    jassert(! icpts.empty());
    jassert(state == Releasing || isPositiveAndBelow(sustainIndex, (int) icpts.size()));

    if (icpts.empty() || (state != Releasing && ! isPositiveAndBelow(sustainIndex, (int) icpts.size()))) {
        DBG(String::formatted("EnvRasterizer[%s] simulateRender invalid state mesh=%p icpts=%d sustain=%d state=%d",
                              MeshRasterizer::getName().toRawUTF8(),
                              envMesh,
                              (int) icpts.size(),
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

    double boundary = Rasterization::EnvelopePlaybackPolicy().boundary(icpts, context);
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
        releaseContext.bipolar = getScalingType() == Bipolar;
        releaseContext.sustainIndex = sustainIndex;

        int releaseIdx = Rasterization::EnvelopeReleasePolicy().releaseIndex(releaseContext);
        lastPosition = icpts[releaseIdx].x;
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
    auto markers = Rasterization::EnvRasterizerFacade().evaluateMarkers(icpts, envMesh, loopMinSizeIcpts);
    loopIndex = markers.loopIndex;
    sustainIndex = markers.sustainIndex;
}

void EnvRasterizer::changedToRelease() {
    jassert(state == Releasing);

    dbg("recalculating release");

    if (canLoop()) {
        waveform.waveX = waveXCopy;
        waveform.waveY = waveYCopy;
        waveform.slope = slopeCopy;
    }

    float lastLevel = params[headUnisonIndex].sustainLevel;

    Rasterization::EnvelopeReleaseContext releaseContext;
    releaseContext.bipolar = getScalingType() == Bipolar;
    releaseContext.sustainIndex = sustainIndex;

    int releaseIdx = Rasterization::EnvelopeReleasePolicy().releaseIndex(releaseContext);
    float sampledReleaseLevel = sampleAt(icpts[releaseIdx].x);
    auto release = Rasterization::EnvelopeReleasePolicy().start(
            icpts,
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
    context.loopStart = loopIndex >= 0 && loopIndex < (int) icpts.size() ? icpts[loopIndex].x : 0.;
    context.loopEnd = sustainIndex >= 0 && sustainIndex < (int) icpts.size() ? icpts[sustainIndex].x : 0.;

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

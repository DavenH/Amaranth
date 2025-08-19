#include "EnvRasterizer.h"

#include <Definitions.h>

#include "../../App/SingletonRepo.h"
#include "../../Util/Arithmetic.h"
#include "../Env/EnvelopeMesh.h"


EnvRasterizer::EnvRasterizer(SingletonRepo* repo, IPathSampler* path, const String& name) :
        SingletonAccessor(repo, name)
    ,   OldMeshRasterizer(name)
    ,    envMesh         (nullptr)
    ,    sampleReleaseNextCall(false)
    ,    oneSamplePerCycle(false)
    ,    state           (NormalState)
    ,    loopIndex       (-1)
    ,    sustainIndex    (-1)
    ,    releaseScale    (1.f)
    ,    preallocated    (8192)
    ,    waveformMemory  (4096) {
    cyclic = false;
    xMaximum = 10.f;

    // graphic params
    params.emplace_back();

    // head unison index
    params.emplace_back();

    paddingSize = 2;

    setPath(path);
}

EnvRasterizer& EnvRasterizer::operator=(const EnvRasterizer& copy) {
    OldMeshRasterizer::operator =(copy);

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

EnvRasterizer::EnvRasterizer(const EnvRasterizer& copy)
    : SingletonAccessor(copy),
      OldMeshRasterizer(copy) {
    initialise();
    operator=(copy);
}

EnvRasterizer::~EnvRasterizer() {
    preallocated.clear();
    waveformMemory.clear();
}

void EnvRasterizer::setMesh(Mesh* mesh) {
    jassertfalse;

    OldMeshRasterizer::setMesh(mesh);

    envMesh = dynamic_cast<EnvelopeMesh*>(mesh);
}

void EnvRasterizer::setMesh(EnvelopeMesh* envelopeMesh) {
    OldMeshRasterizer::setMesh(envelopeMesh);

    envMesh = envelopeMesh;
}

bool EnvRasterizer::hasReleaseCurve() {
    if (controlPoints.empty()) {
        return false;
    }

    return sustainIndex != (int) controlPoints.size() - 1;
}

void EnvRasterizer::generateControlPoints() {
    // the actual 'time' axis is 'phase'
    morph.time = 0;

    OldMeshRasterizer::generateControlPoints(envMesh, 0.f);

    // do this even if we can't loop right now just in case
    // loopability changes by the time release curve is going
    // to be recalculated
    int size = waveX.size();

    if (size > 0) {
        waveformMemory.ensureSize(3 * size);

        waveXCopy = waveformMemory.place(size);
        waveYCopy = waveformMemory.place(size);
        slopeCopy = waveformMemory.place(size);

        waveX.copyTo(waveXCopy);
        waveY.copyTo(waveYCopy);
        slope.copyTo(slopeCopy);
    } else {
        waveXCopy.nullify();
        waveYCopy.nullify();
        slopeCopy.nullify();
    }

    //    evaluateLoopSustainIndices();
}

// this is the only specializer of `OldMeshRasterizer::processIntercepts`
void EnvRasterizer::processIntercepts(vector<Intercept>& intercepts) {
    evaluateLoopSustainIndices();

    if (scalingType != Bipolar) {
        if (sustainIndex >= 0 && sustainIndex != intercepts.size() - 1) {
            Intercept sustIcpt = intercepts[sustainIndex];
            sustIcpt.cube      = nullptr;
            sustIcpt.x += 0.0001f;

            if (sustIcpt.y < 0.5f) {
                sustIcpt.y = 0.5f;
            }

            sustIcpt.shp = 1.f;

            intercepts.emplace_back(sustIcpt);

            needsResorting = true;
        }
    }
}

void EnvRasterizer::padControlPointsForRender(vector<Intercept>& intercepts, vector<CurvePiece>& curves) {
    int end = (int) intercepts.size() - 1;

    if (end <= 0) {
        waveX.nullify();
        waveY.nullify();
        unsampleable = true;

        return;
    }

    Intercept loopBackIcptA, loopBackIcptB;

    int numLoopPoints = sustainIndex - loopIndex;
    bool loopable     = state != Releasing && canLoop() && numLoopPoints > 1;
    float loopLength  = loopable ? getLoopLength() : 0;

    if (loopable) {
        jassert(loopIndex + loopMinSizeIcpts <= sustainIndex);

        loopBackIcptA = Intercept(intercepts[loopIndex + 1]);
        loopBackIcptA.x += loopLength;

        loopBackIcptB = Intercept(intercepts[loopIndex + 2]);
        loopBackIcptB.x += loopLength;
    }

    if (state == NormalState || state == Releasing) {
        Intercept front1(-0.05f, intercepts[0].y);
        Intercept front2(-0.075f, intercepts[0].y);
        Intercept front3(-0.1f, intercepts[0].y);
        Intercept back1, back2, back3;

        const Intercept& endIcpt = intercepts[end];

        if (state == NormalState) {
            if (loopable) {
                back1 = loopBackIcptA;
                back2 = numLoopPoints > 1 ? loopBackIcptB : loopBackIcptA;
            } else {
                back1 = Intercept(endIcpt.x + 0.001f, endIcpt.y);
                back2 = Intercept(endIcpt.x + 0.002f, endIcpt.y);
                back3 = Intercept(endIcpt.x + 2.5f, endIcpt.y);
            }
        } else {
            back1 = Intercept(endIcpt.x + 0.001f, endIcpt.y);
            back2 = Intercept(endIcpt.x + 0.002f, endIcpt.y);
            back3 = Intercept(endIcpt.x + 0.003f, endIcpt.y);
        }

        curves.clear();
        curves.reserve(controlPoints.size() + 6);

        curves.emplace_back(front3, front2, front1);
        curves.emplace_back(front2, front1, intercepts[0]);
        curves.emplace_back(front1, intercepts[0], intercepts[1]);

        for (int i = 0; i < (int) controlPoints.size() - 2; ++i) {
            curves.emplace_back(intercepts[i], intercepts[i + 1], intercepts[i + 2]);
        }

        curves.emplace_back(intercepts[end - 1], intercepts[end], back1);
        curves.emplace_back(intercepts[end], back1, back2);

        if (state == Releasing || !loopable) {
            curves.emplace_back(back1, back2, back3);
        }
    } else if (state == Looping && loopable) {
        vector<Intercept> loopIcpts;

        jassert(envMesh && ! envMesh->loopCubes.empty());
        jassert(loopIndex >= 0);
        jassert(loopLength > 0);
        jassert(sustainIndex >= 2);

        loopIcpts.emplace_back(intercepts[sustainIndex - 2]);
        loopIcpts.emplace_back(intercepts[sustainIndex - 1]);

        loopIcpts[0].x -= loopLength;
        loopIcpts[1].x -= loopLength;

        for (int i = loopIndex; i <= sustainIndex; ++i) {
            loopIcpts.emplace_back(intercepts[i]);
        }

        loopIcpts.emplace_back(loopBackIcptA);
        loopIcpts.emplace_back(loopBackIcptB);

        curves.clear();
        curves.reserve(loopIcpts.size());

        for (int i = 0; i < (int) loopIcpts.size() - 2; ++i) {
            curves.emplace_back(loopIcpts[i], loopIcpts[i + 1], loopIcpts[i + 2]);
        }
    } else {
        OldMeshRasterizer::padControlPoints(intercepts, curves);
    }
}

void EnvRasterizer::padControlPoints(vector<Intercept>& controlPoints, vector<CurvePiece>& curves) {
    int end = (int) controlPoints.size() - 1;

    if (end <= 0) {
        waveX.nullify();
        waveY.nullify();
        unsampleable = true;
        return;
    }

    Intercept& firstIcpt = controlPoints[0];

    Intercept front1(-0.05f, firstIcpt.y);
    Intercept front2(-0.075f, firstIcpt.y);
    Intercept front3(-0.1f, firstIcpt.y);
    Intercept back1, back2, back3;

    const Intercept& endIcpt = controlPoints[end];

    bool loopable    = canLoop();
    bool haveRelease = hasReleaseCurve();

    int numLoopPoints = sustainIndex - loopIndex;
    if (loopable && !haveRelease && numLoopPoints >= 2) {
        back1 = Intercept(controlPoints[loopIndex + 1]);
        back2 = Intercept(controlPoints[loopIndex + 2]);

        float loopLength = getLoopLength();
        back1.x += loopLength;
        back2.x += loopLength;
    } else {
        back1 = Intercept(endIcpt.x + 0.001f, endIcpt.y, 0, 0);
        back2 = Intercept(endIcpt.x + 0.002f, endIcpt.y, 0, 0);
    }

    // further padding to ensure the endIcpt.x position is sampleable
    back3 = Intercept(endIcpt.x + 0.003f, endIcpt.y, 0, 0);

    curves.clear();
    curves.reserve(controlPoints.size() + 5);

    curves.emplace_back(front3, front2, front1);
    curves.emplace_back(front2, front1, controlPoints[0]);
    curves.emplace_back(front1, controlPoints[0], controlPoints[1]);

    for (int i = 0; i < (int) controlPoints.size() - 2; ++i) {
        curves.emplace_back(controlPoints[i], controlPoints[i + 1], controlPoints[i + 2]);
    }

    curves.emplace_back(controlPoints[end - 1], controlPoints[end], back1);
    curves.emplace_back(controlPoints[end], back1, back2);

    if (haveRelease || !loopable) {
        curves.emplace_back(back1, back2, back3);
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
            DeformContext& context  = param.deformContext;
            context.phaseOffsetSeed = rand.nextInt(tableSize);
            context.vertOffsetSeed  = rand.nextInt(tableSize);
        }
    } else {
        OldMeshRasterizer::updateOffsetSeeds(layerSize, tableSize);
    }
}

void EnvRasterizer::setWantOneSamplePerCycle(bool does) {
    oneSamplePerCycle      = does;
    decoupleComponentPaths = does;
}

bool EnvRasterizer::canLoop() const {
    return loopIndex >= 0; // && sustainIndex - loopIndex >= loopMinSizeIcpts;
}

float EnvRasterizer::getLoopLength() const {
    if (loopIndex >= 0 && loopIndex < (controlPoints.size() - 1)
        && sustainIndex >= 0 && sustainIndex < controlPoints.size()) {
        return controlPoints[sustainIndex].x - controlPoints[loopIndex].x;
    }

    return -1.f;
}

void EnvRasterizer::setNoteOn() {
    state = NormalState;

    dbg("\tnote on for " << OldMeshRasterizer::name);

    sampleReleaseNextCall = false;

    for (int i = headUnisonIndex; i < (int) params.size(); ++i) {
        params[i].reset();
    }
}

void EnvRasterizer::setNoteOff() {
    if (state != Releasing) {
        if (hasReleaseCurve()) {
            state                 = Releasing;
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

    double newDelta = deltaX;

    if (props.tempoSync) {
        newDelta /= tempoScale;
    }

    if (props.scale != 1) {
        newDelta /= (double) props.getEffectiveScale();
    }

    if (!oneSamplePerCycle) {
        // should happen extremely rarely�only when buffer sizes > 8192
        preallocated.ensureSize(numSamples);
        renderBuffer = preallocated.withSize(numSamples);
    }

    // todo adjust this according to loop length? must be less than loop length in time
    // also depends on deltaX, if it's very large, the granularity must be increased
    auto loopLength               = jmax<float>(2.f * newDelta, getLoopLength()); //88
    float maxIterativeAdvancement = 0.5f;
    int maxSamplesPerBuf          = 512;

    if (loopLength > 2 * newDelta) {
        maxIterativeAdvancement = 0.9f * loopLength; //130
    }

    maxSamplesPerBuf = jlimit(1, maxSamplesPerBuf, int(maxIterativeAdvancement / newDelta));

    int bufferPos   = 0;
    bool stillAlive = true;

    // partition buffers because state may change within buffer
    while (numSamples - bufferPos > 0) {
        int maxSamples      = jmin(maxSamplesPerBuf, numSamples - bufferPos);
        int samplesRendered = maxSamples;

        Buffer<float> buffer;
        if (!oneSamplePerCycle) {
            buffer = Buffer(renderBuffer + bufferPos, maxSamples);
        }

        if (stillAlive) {
            samplesRendered = vectorizedRenderToBuffer(buffer, maxSamples, newDelta, paramIndex);

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

    dbg("\n\n" << OldMeshRasterizer::name << "(" << paramIndex << ")");

    double advancement        = numSamples * deltaX;
    double boundary           = (state == Releasing) ? controlPoints.back().x : controlPoints[sustainIndex].x;
    double nextPosition       = group.samplePosition + advancement;
    bool overextends          = nextPosition > boundary;
    bool loopable             = canLoop();
    bool willLoopBackNextCall = loopable && overextends && state == NormalState;
    int samplesRendered       = numSamples;

    dbg("rendering to buffer\t" << buffer.size() << " with delta " << deltaX <<
        ", range " << group.samplePosition << " - " << nextPosition);

    if (willLoopBackNextCall) {
        dbg("calculating loop curves, state = looping");
        state = Looping;

        padControlPointsForRender(controlPoints, curves);
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
                group.sustainLevel = sampleAtDecoupled(group.samplePosition, group.deformContext);
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
                group.sustainLevel = sampleAtDecoupled(group.samplePosition, group.deformContext);

                while (overextends) {
                    group.samplePosition -= loopLength;
                    overextends = group.samplePosition + advancement > boundary;
                }

                group.samplePosition = jmin(boundary, group.samplePosition + advancement);
            } else {
                jassert(numSamples > 0);

                group.sampleIndex = zeroIndex;

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
                    group.sustainLevel = releaseScale * sampleAtDecoupled(group.samplePosition, group.deformContext);
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
    lastPosition          = 0;
    state                 = NormalState;
}

bool EnvRasterizer::simulateStop(double& lastPosition) {
    setNoteOff();

    if (hasReleaseCurve()) {
        lastPosition = controlPoints[sustainIndex].x;
        return true;
    }

    return false;
}

bool EnvRasterizer::simulateRender(
    double advancement,
    double& lastPosition,
    const MeshLibrary::EnvProps& props,
    float tempoScale) {
    double newAdvancement = advancement;

    if (props.tempoSync) {
        newAdvancement /= tempoScale;
    }

    if (props.scale != 1) {
        newAdvancement /= (double) props.getEffectiveScale();
    }

    advancement = newAdvancement;

    double boundary           = (state == Releasing) ? controlPoints.back().x : controlPoints[sustainIndex].x;
    double nextPosition       = lastPosition + advancement;
    bool overextends          = nextPosition > boundary;
    bool loopable             = canLoop();
    bool willLoopBackNextCall = loopable && overextends && state == NormalState;

    if (willLoopBackNextCall) {
        state = Looping;
    }

    if (sampleReleaseNextCall) {
        sampleReleaseNextCall = false;
        int releaseIdx        = scalingType == Bipolar ? sustainIndex : sustainIndex + 1;
        float releasePosX     = controlPoints[releaseIdx].x;
        lastPosition          = releasePosX;
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
    //    progressMark

    int end = (int) controlPoints.size() - 1;

    loopIndex    = -1;
    sustainIndex = -1;

    if (controlPoints.empty()) {
        return;
    }

    set<TrilinearCube*>& loopLines = envMesh->loopCubes;
    set<TrilinearCube*>& sustLines = envMesh->sustainCubes;

    for (int i = 0; i < (int) controlPoints.size(); ++i) {
        TrilinearCube* cube = controlPoints[i].cube;

        if (cube == nullptr) {
            continue;
        }

        if (loopLines.find(cube) != loopLines.end()) {
            loopIndex = i;
        }

        if (sustLines.find(cube) != sustLines.end()) {
            sustainIndex = i;
        }
    }

    if (sustainIndex < 0) {
        sustainIndex = end;
    }

    if (loopIndex >= 0 && sustainIndex >= 0 && sustainIndex - loopIndex < loopMinSizeIcpts) {
        // need at least 1 icpt padding to the right to loop
        loopIndex = -1;
    }
}

void EnvRasterizer::changedToRelease() {
    jassert(state == Releasing);

    dbg("recalculating release");

    if (canLoop()) {
        waveX = waveXCopy;
        waveY = waveYCopy;
        slope = slopeCopy;
    }

    float lastLevel           = params[headUnisonIndex].sustainLevel;
    int releaseIdx            = scalingType == Bipolar ? sustainIndex : sustainIndex + 1;
    float releasePosX         = controlPoints[releaseIdx].x;
    float initialReleaseLevel = jmax(0.5f, sampleAt(releasePosX));

    releaseScale = lastLevel / initialReleaseLevel;

    dbg("release scale: " << releaseScale);

    for (int i = headUnisonIndex; i < (int) params.size(); ++i) {
        params[i].samplePosition = releasePosX;
    }
}

void EnvRasterizer::validateState() {
    if (waveX.empty()) {
        state = NormalState;
    }

    for (auto& p: params) {
        if (waveX.empty()) {
            p.samplePosition = 0;
            p.sampleIndex    = 0;
        } else {
            if (p.samplePosition > waveX.back()) {
                p.samplePosition = waveX.back();
                p.sampleIndex    = waveX.size() - 1;
            }
        }
    }

    if (state == Looping) {
        float loopLength = getLoopLength();

        if (loopLength <= 0) {
            state = NormalState;
        } else {
            for (int i = headUnisonIndex; i < params.size(); ++i) {
                double low  = controlPoints[loopIndex].x;
                double high = controlPoints[sustainIndex].x;

                double& pos = params[i].samplePosition;

                if (pos > high) {
                    pos = jmax(low, pos - loopLength);
                } else if (pos < low) {
                    pos = jmin(high, pos + loopLength);
                }
            }
        }
    } else if (state == NormalState) {
        float length = getLoopLength();

        if (length > 0) {
            if (NumberUtils::within((float) params[headUnisonIndex].samplePosition,
                                    controlPoints[loopIndex].x, controlPoints[sustainIndex].x)) {
                state = Looping;
            }
        }
    }
}

void EnvRasterizer::ensureParamSize(int numUnisonVoices) {
    if (params.size() == numUnisonVoices + 1) {
        return;
    }

    while (params.size() < numUnisonVoices + 1) {
        params.emplace_back();
    }
}

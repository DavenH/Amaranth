#include <Algo/Resampler.h>
#include <Curve/EnvRasterizer.h>
#include <Curve/IDeformer.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegions.h>
#include <Util/NumberUtils.h>
#include <Util/Util.h>

#include "CycleBasedVoice.h"

#include <climits>
#include <Util/StatusChecker.h>

#include "SynthesizerVoice.h"
#include "Algo/Resampling.h"
#include "../SynthAudioSource.h"
#include "../../App/Initializer.h"
#include "../../Audio/Effects/Unison.h"
#include "../../Curve/EnvRenderContext.h"
#include "../../UI/Panels/ModMatrixPanel.h"
#include "../../UI/VertexPanels/Envelope2D.h"
#include "../../UI/VertexPanels/Spectrum3D.h"
#include "../../UI/VertexPanels/DeformerPanel.h"
#include "../../UI/VertexPanels/Waveform3D.h"
#include "../../Util/CycleEnums.h"
#include "../CycleDefs.h"

CycleBasedVoice::CycleBasedVoice(SynthesizerVoice* parent, SingletonRepo* repo) :
        SingletonAccessor	(repo, "CycleBasedVoice")
    ,	parent				(parent)
    , 	random				(Time::currentTimeMillis())
    , 	oversampleAccumBuf	(2)
    , 	cycleCompositeAlgo	(Chain)
    , 	resamplingAlgo		(Resampling::Hermite)
    ,	timeRasterizer		(repo) {
    waveform3D = &getObj(Waveform3D);
    spectrum3D = &getObj(Spectrum3D);
    envelope2D = &getObj(Envelope2D);
    audioSource = &getObj(SynthAudioSource);

    unison = &audioSource->getUnison();
    timeLayers = &getObj(MeshLibrary).getLayerGroup(LayerGroups::GroupTime);

    int unisonVoices = unison->getOrder(true);
    noteState.numUnisonVoices = unisonVoices;

    random.combineSeed(parent->voiceIndex * 100 * getObj(Initializer).getInstanceId());

    const int maxPeriod = getConstant(MaxCyclePeriod);
    int half = maxPeriod / 2;

    rastBuffer.resize(maxPeriod);
    tempBuffer.resize(maxPeriod);

    timeRasterizer.setCalcDepthDimensions(false);
    timeRasterizer.setDeformer(&getObj(DeformerPanel));

    for (int c = 0; c < 2; ++c) {
        // we reuse this as a CCS fft buffer -> 2(n + 1) samples
        layerAccumBuffer[c].resize(maxPeriod + 2);
        pastCycle[c].resize(maxPeriod);
        biasedCycle[c].resize(maxPeriod);

        oversamplers.add(new Oversampler(repo));
    }

    unisonVoiceCountChanged();

    halfBuf.resize(half);
}

void CycleBasedVoice::initialiseNote(const int midiNoteNumber, const float velocity) {
    noteState.lastNoteNumber = midiNoteNumber;
    bool unisonEnabled = unison == nullptr ? false : unison->isEnabled();
    bool interpolatingNotUpdating = cycleCompositeAlgo == Interpolate;

    jassert(noteState.lastNoteNumber > 0);

    testNumLayersChanged();
    testIfOversamplingChanged();
    testIfResamplingQualityChanged();

    if (Util::assignAndWereDifferent(noteState.numUnisonVoices, audioSource->getNumUnisonVoices()) && unisonEnabled) {
        unisonVoiceCountChanged();
    }

    const int timeLayerSize = timeLayers->size();
    timeRasterizer.updateOffsetSeeds(timeLayerSize, DeformerPanel::tableSize);

    EnvRasterizer& pitchRast = parent->pitchGroup[0].rast;
    float pitchEnvVal = parent->flags.havePitch ? pitchRast.sampleAt(0) : 0.5;

    for (int i = 0; i < noteState.numUnisonVoices; ++i) {
        float unisonTune = unisonEnabled ? unison->getDetune(i) : 0;

        VoiceParameterGroup& group = groups[i];
        group.reset();
        group.angleDelta = getAngleDelta(midiNoteNumber, unisonTune, pitchEnvVal);
    }

    noteState.reset();
    futureFrame.reset();
    frame.reset();

    double middleDelta = getAngleDelta(noteState.lastNoteNumber, 0, 0.5f);
    double middlePeriod = 1 / middleDelta;
    int controlFreq = audioSource->getControlFreq();
    noteState.numHarmonics = getObj(LogRegions).getRegion(noteState.lastNoteNumber).size();

    // this power of two is unaffected by pitch and unison detune
    noteState.nextPow2 = Arithmetic::getNextPow2(middlePeriod);

    if (noteState.nextPow2 == 0) {
        noteState.nextPow2 = Arithmetic::getNextPow2(middlePeriod);
    }

    for (int i = 0; i < 2; ++i) {
        oversamplers[i]->resetDelayLine();
    }

    noteState.stride = jmax(1, (int) (controlFreq / futureFrame.period + 0.5));
    futureFrame.cycleCount = -noteState.stride;
    futureFrame.period = middlePeriod;

    if (parent != nullptr) {
        // remember envelopes are unaffected by speed mesh distortion
        noteState.timePerOutputSample = parent->speedScale.getCurrentValue() / 44100.0;
    }

    initialiseNoteExtra(midiNoteNumber, velocity);

    for (int i = 0; i < noteState.numUnisonVoices; ++i) {
        for (auto& c: groups[i].padding) {
            for (float& j : c) {
                j = 0;
            }
        }
    }

    if (resamplingAlgo == Resampling::Sinc) {
        const float windowAlpha = 9.f;
        const float rolloffRelFreq = 0.95f;
        const int numPolyphaseSteps = 256;
        const int resampleBufferSize = getConstant(MaxCyclePeriod) + 16;

        int totalSize = 0;
        vector<int> sourceSizes;
        vector<int> destSizes;

        for (int i = 0; i < noteState.numUnisonVoices; ++i) {
            int sourceSize, destSize;
            double outputToInputRate = 1. / (double(noteState.nextPow2) * groups[i].angleDelta);
          #ifdef USE_IPP
            groups[i].resamplers[Left].initWithHistory(rolloffRelFreq, windowAlpha, outputToInputRate,
                                                       getConstant(ResamplerLatency), numPolyphaseSteps,
                                                       resampleBufferSize, sourceSize, destSize);

            groups[i].resamplers[Right].initWithHistory(rolloffRelFreq, windowAlpha, outputToInputRate,
                                                        getConstant(ResamplerLatency), numPolyphaseSteps,
                                                        resampleBufferSize, sourceSize, destSize);
          #endif

            sourceSizes.push_back(sourceSize);
            destSizes.push_back(destSize);

            totalSize += 2 * (sourceSize + destSize);
        }

        resamplingMemory.ensureSize(totalSize);

        for (int i = 0; i < noteState.numUnisonVoices; ++i) {
            for (int c = 0; c < 2; ++c) {
                Resampler& resampler = groups[i].resamplers[c];
              #ifdef USE_IPP
                resampler.source = resamplingMemory.place(sourceSizes[i]);
                resampler.dest = resamplingMemory.place(destSizes[i]);
                resampler.reset();
                resampler.primeWithZeros();
              #endif
                // pad with one sample to eliminate possibility of resampling problems
                groups[i].cycleBuffer[c].write(0);
            }
        }
    }
}

void CycleBasedVoice::initCycleBuffers() {
    int bufferSize = getObj(AudioHub).getBufferSize();
    int cycleBuffSize = bufferSize + oversamplers[0]->getOversampleFactor() * getConstant(MaxCyclePeriod);
    int totalSize = 2 * groups.size() * cycleBuffSize;
    int offset = 0;

    if (cycleBufferMemory.ensureSize(totalSize))
        cycleBufferMemory.zero(totalSize);

    for (auto& group: groups) {
        for (auto& c: group.cycleBuffer) {
            Buffer<float> buf = cycleBufferMemory.section(offset, cycleBuffSize);

            c.reset();
            c.setMemoryBuffer(buf);

            // pad with one sample to eliminate possibility of resampling problems
            c.write(0);

            offset += cycleBuffSize;
        }
    }
}

void CycleBasedVoice::render(StereoBuffer& channelPair) {
    int numSamples = channelPair.left.size();

    if (parent && parent->flags.latencyFilling) {
        return;
    }

    switch (cycleCompositeAlgo) {
        case Chain:
            renderChainedCycles(numSamples);
            break;

        case Interpolate:
            renderInterpolatedCycles(numSamples);
            break;

        case Overlap:
            renderOverlappedCycles(numSamples);
            break;
        default: break;
    }

    int oversampleFactor = oversamplers[0]->getOversampleFactor();
    bool downSample = cycleCompositeAlgo == Chain && oversampleFactor > 1;
    int srcChanCount = noteState.isStereo ? 2 : 1;

    if (downSample) {
        for (int c = 0; c < srcChanCount; ++c) {
            oversampleAccumBuf[c].zero();
        }
    }

    StereoBuffer& sterBufToUse = downSample ? oversampleAccumBuf : channelPair;
    commitCycleBufferAndWrap(sterBufToUse);

    int dstChanCount = channelPair.numChannels;

    if (downSample) {
        if (srcChanCount == dstChanCount) {
            for (int c = 0; c < srcChanCount; ++c)
                oversamplers[c]->sampleDown(oversampleAccumBuf[c], channelPair[c]);
        } else if (srcChanCount == 1) {
            oversamplers[0]->sampleDown(oversampleAccumBuf[Left], channelPair.left);
            channelPair.left.copyTo(channelPair.right);
        } else if (dstChanCount == 1) {
            ScopedAlloc<Float32> tempBuffer(numSamples);
            oversamplers[0]->sampleDown(oversampleAccumBuf[Left], tempBuffer);
            oversamplers[1]->sampleDown(oversampleAccumBuf[Right], channelPair.left);

            channelPair.left.mul(0.5f).addProduct(tempBuffer, 0.5f);
        }
    }

    noteState.totalSamplesPlayed += numSamples;
}

void CycleBasedVoice::renderChainedCycles(int numSamples) {
    long lastIndexToRender = noteState.totalSamplesPlayed + (long) numSamples;
    ensureOversampleBufferSize(numSamples);

    bool unisonEnabled = unison == nullptr ? false : unison->isEnabled();
    double dperiod = 0;

    for (int i = 0; i < noteState.numUnisonVoices; ++i) {
        VoiceParameterGroup& group = groups[i];

        while (group.sampledFrontier < lastIndexToRender) {
            if (parent != nullptr) {
                int period = int(1 / group.angleDelta + 0.5);
                noteState.absVoiceTime = jmin(group.cumePos * noteState.timePerOutputSample, 1.);

                int deltaSamples = group.samplesThisCycle;

                if (deltaSamples == 0)
                    deltaSamples = period;

                updateEnvelopes(EnvRasterizer::headUnisonIndex + group.unisonIndex, deltaSamples);
            }

            updateChainAngleDelta(group, unisonEnabled);

            dperiod = 1. / group.angleDelta;
            int floorCume = int(group.cumePos);
            float nextCume = group.cumePos + dperiod;
            int floorNextCume = int(nextCume);

            group.samplesThisCycle = floorNextCume - floorCume;

            calcCycle(group);

            group.cumePos = nextCume;
            group.sampledFrontier = floorNextCume;

            int ovspCycleSize = group.samplesThisCycle * oversamplers[0]->getOversampleFactor();

            for (int c = 0; c < 2; ++c)
                group.cycleBuffer[c].write(layerAccumBuffer[c].withSize(ovspCycleSize));
        }
    }
}

void CycleBasedVoice::renderOverlappedCycles(int numSamples) {
    long lastIndexToRender = noteState.totalSamplesPlayed + (long) numSamples;
    ensureOversampleBufferSize(numSamples);

    bool unisonEnabled = unison == nullptr ? false : unison->isEnabled();
    double dperiod = 0;

    VoiceParameterGroup& group = groups.front();
    noteState.numUnisonVoices = 1;

    while (group.sampledFrontier < lastIndexToRender) {
        if (parent != nullptr) {
            int period = 1 / group.angleDelta;
            noteState.absVoiceTime = jmin(group.cumePos * noteState.timePerOutputSample, 1.);

            int deltaSamples = group.samplesThisCycle;

            if (deltaSamples == 0)
                deltaSamples = period;

            updateEnvelopes(EnvRasterizer::headUnisonIndex + group.unisonIndex, deltaSamples);
        }

        updateChainAngleDelta(group, unisonEnabled);

        dperiod = 1. / group.angleDelta;

        int floorCume = int(group.cumePos);
        float nextCume = group.cumePos + dperiod;
        int floorNextCume = int(nextCume);
        group.samplesThisCycle = floorNextCume - floorCume;

        calcCycle(group);

        group.cumePos = nextCume;
        group.sampledFrontier = floorNextCume;

        int ovspCycleSize = group.samplesThisCycle * oversamplers[0]->getOversampleFactor();

        for (int c = 0; c < 2; ++c)
            group.cycleBuffer[c].write(layerAccumBuffer[c].withSize(ovspCycleSize));
    }
}

/*

while(futureFrame.frontier < last)
{
    bool isSaturated = singleFrame ?
            midFrame.cycleCount == futureFrame.cycleCount :
            midFrame.cumePos >= futureFrame.cumePos;

    if(isSaturated || futureFrame.cycleCount < 0)
    {
        futureFrame.period = 1 / getAngle(x, 0);

        long pastPos = futureFrame.cumePos;

        if(futureFrame.cycleCount > 0)
            futureFrame.cumePos += futureFrame.period * stride;

        long samplesAdvanced = (long) futureFrame.cumePos - pastPos;

        x = jmin(1., futureFrame.cumePos / 44100.0);

        dout 	<< "refreshing, " << futureFrame.cumePos << ", " << x << ", "
                << futureFrame.cycleCount << ", " << samplesAdvanced << "\n";

        refresh();

        futureFrame.cycleCount += stride;
        futureFrame.frontier 	= (long) futureFrame.cumePos;
    }

    for(int i = 0; i < unisonCount; ++i)
    {
        RenderFrame& frame = frames[i];

        bool condition = singleFrame ? frame.cycleCount < futureFrame.cycleCount :
                                       frame.cumePos < futureFrame.cumePos;

        while(condition && frame.frontier < last)
        {
            int rem 		= frame.cycleCount % stride;
            float portion 	= singleFrame ? rem / float(stride) :
                              1 - (futureFrame.cumePos - frame.cumePos) / float(stride * futureFrame.period);

            frame.period	= 1 / getAngle(x, 2 - i);
            frame.cumePos  += frame.period;
            frame.frontier 	= (long) frame.cumePos;

            dout << "cycle: " << i << ", " << portion << ", " << frame.cumePos << ", " << frame.frontier << "\n";

            ++frame.cycleCount;

            combine();
        }
    }

    futureFrame.frontier = midFrame.frontier;
}
*/

void CycleBasedVoice::renderInterpolatedCycles(int numSamples) {
    bool unisonEnabled = unison->isEnabled();
    bool singleFrame = noteState.numUnisonVoices == 1;

    int half = noteState.nextPow2 / 2;
    int sizeIndex = audioSource->sizeToIndex[noteState.nextPow2];
    long lastSampleToRender = noteState.totalSamplesPlayed + (long) numSamples;

    while (frame.frontier < lastSampleToRender) {
        bool isSaturated = singleFrame
                               ? frame.cycleCount == futureFrame.cycleCount
                               : frame.cumePos >= futureFrame.cumePos;

        if (isSaturated || futureFrame.cycleCount < 0) {
            futureFrame.period = 1 / getAngleDelta(noteState.lastNoteNumber, 0, 0.5f);
            long pastPos = futureFrame.cumePos;

            if (futureFrame.cycleCount >= 0) {
                futureFrame.cumePos += futureFrame.period * noteState.stride;
            }

            long samplesAdvanced = jmax(1L, (long) futureFrame.cumePos - pastPos);

            // NB this position is for the future cycle
            noteState.absVoiceTime = jmin(futureFrame.cumePos * noteState.timePerOutputSample, 1.);
            futureFrame.cycleCount += noteState.stride;
            futureFrame.frontier = (long) futureFrame.cumePos;

            for (int i = 0; i < noteState.numUnisonVoices; ++i) {
                updateEnvelopes(EnvRasterizer::headUnisonIndex + groups[i].unisonIndex, samplesAdvanced);
                updateChainAngleDelta(groups[i], unisonEnabled, false);
            }

            calcCycle(groups.front());
        }

        bool isFirstCycle = false;
        bool doPhaseShift = parent->flags.haveFilter && noteState.numUnisonVoices > 1 && unison->isPhased();

        Buffer<float> cycleSourceBuf, cyclePrevBuf;

        noteState.isStereo |= noteState.numUnisonVoices > 1 && unison->isStereo();

        long minFrontier = INT_MAX;

        for (int i = 0; i < noteState.numUnisonVoices; ++i) {
            VoiceParameterGroup& group = groups[i];

            double dperiod = 1 / group.angleDelta;
            double srcToDestRatio = noteState.nextPow2 * group.angleDelta;
            float uniPan = unison->getPan(group.unisonIndex);
            float pans[2] = { 1, 0 };

            if (i == 0) {
                noteState.lastPow2Ratio = srcToDestRatio;
            }

            jassert(srcToDestRatio < 3.);

            NumberUtils::constrain(uniPan, 0.f, 1.f);
            Arithmetic::getPans(uniPan, pans[0], pans[1]);

            while (group.sampledFrontier < lastSampleToRender &&
                   (singleFrame ? frame.cycleCount < futureFrame.cycleCount : group.cumePos < futureFrame.cumePos)) {
                int truncCume = resamplingAlgo == Resampling::Sinc ? ceil(group.cumePos) : int(group.cumePos);
                double nextCume = group.cumePos + dperiod;
                int truncNextCume = resamplingAlgo == Resampling::Sinc ? ceil(nextCume) : int(nextCume);
                isFirstCycle = truncCume == 0;

                group.samplesThisCycle = truncNextCume - truncCume;
                group.sampledFrontier = truncNextCume;

                int remainder = frame.cycleCount % noteState.stride;
                float portionOfNext = singleFrame
                                          ? remainder / float(noteState.stride)
                                          : 1 - (futureFrame.cumePos - group.cumePos) / float(
                                                noteState.stride * futureFrame.period);

                //				jassert(portionOfNext <= 1.5f && portionOfNext >= -0.5f);
                NumberUtils::constrain(portionOfNext, 0.f, 1.f);
                group.cumePos = nextCume;

                if (i == 0)
                    frame.cumePos = group.cumePos;

                int scaledPhase = int(noteState.nextPow2 * (unison->getPhase(group.unisonIndex))) & (
                                      noteState.nextPow2 - 1);

                //				dout << "uni vox: " << i << ", " << portionOfNext << ", " << frame.cumePos << ", " << frame.frontier << "\n";

                ++frame.cycleCount;

                Buffer<float> destBuffer;
                Buffer halfBuff(halfBuf.withSize(half));

                for (int c = 0; c < (noteState.isStereo ? 2 : 1); ++c) {
                    Buffer<float> srcBuffer;
                    Buffer biasedCyc(biasedCycle[c].withSize(noteState.nextPow2));
                    Buffer lerpHalf(group.lastLerpHalf[c].withSize(half));
                    Buffer pastCyc(pastCycle[c].withSize(noteState.nextPow2));

                    // no fading needed on first cycle
                    if (isFirstCycle && !doPhaseShift) {
                        srcBuffer = pastCyc;
                    } else {
                        biasedCyc.zero();

                        if (doPhaseShift && scaledPhase != 0) {
                            if (isFirstCycle) {
                                pastCycle[c]
                                    .withSize(noteState.nextPow2 - scaledPhase)
                                    .copyTo(biasedCyc + scaledPhase);
                                pastCycle[c]
                                    .section(noteState.nextPow2 - scaledPhase, scaledPhase)
                                    .copyTo(biasedCyc.withSize(scaledPhase));

                                biasedCyc.copyTo(lerpHalf);
                            } else {
                                layerAccumBuffer[c]
                                    .withSize(noteState.nextPow2 - scaledPhase)
                                    .copyTo(rastBuffer + scaledPhase);
                                layerAccumBuffer[c]
                                    .section(noteState.nextPow2 - scaledPhase, scaledPhase)
                                    .copyTo(rastBuffer.withSize(scaledPhase));
                                pastCyc
                                    .withSize(noteState.nextPow2 - scaledPhase)
                                    .copyTo(tempBuffer + scaledPhase);
                                pastCyc
                                    .section(noteState.nextPow2 - scaledPhase, scaledPhase)
                                    .copyTo(tempBuffer.withSize(scaledPhase));

                                cycleSourceBuf = rastBuffer.withSize(noteState.nextPow2);
                                cyclePrevBuf = tempBuffer.withSize(noteState.nextPow2);
                            }
                        } else {
                            cycleSourceBuf = layerAccumBuffer[c].withSize(noteState.nextPow2);
                            cyclePrevBuf = pastCyc;
                        }

                        // first cycle given special treatment when it comes to compositing
                        if (!isFirstCycle) {
                            if (portionOfNext == 0.f) {
                                cyclePrevBuf.copyTo(biasedCyc);
                            } else {
                                // lerp surrounding cycles
                                biasedCyc.addProduct(cyclePrevBuf, 1 - portionOfNext);
                                biasedCyc.addProduct(cycleSourceBuf, portionOfNext);
                            }

                            // make copy of last composite-cycle's latter half before it is replaced
                            lerpHalf.copyTo(halfBuff);

                            // store half before we distort it with fades
                            biasedCyc.copyTo(lerpHalf);

                            // fade in the incoming cycle, fade out the outgoing cycle, removing discontinuities
                            biasedCyc.mul(audioSource->fadeIns[sizeIndex]);
                            biasedCyc.addProduct(audioSource->fadeOuts[sizeIndex], halfBuff);
                        }

                        srcBuffer = biasedCyc;
                    }

                    switch (resamplingAlgo) {
                        case Resampling::Sinc:
#ifdef USE_IPP
                        {
                            group.resamplers[c].setRatio(1. / srcToDestRatio);
                            Buffer<float> output = group.resamplers[c].resample(srcBuffer);
                            int difference = output.size() - group.samplesThisCycle;

                            if (difference != 0) {
                                group.sampledFrontier += difference;
                            }

                            destBuffer = group.cycleBuffer[c].write(output);
                            break;
                        }
#endif
                        case Resampling::Linear: {
                            Buffer<float> interBuffer = tempBuffer.withSize(group.samplesThisCycle);

                            Resampling::linResample2(srcBuffer, interBuffer, srcToDestRatio,
                                                     group.padding[c][0],
                                                     group.padding[c][1],
                                                     group.samplingSpillover[c]);

                            destBuffer = group.cycleBuffer[c].write(interBuffer);
                        }

                        default: {
                            auto* p = (float*) group.padding[c];

                            Buffer<float> interBuffer = tempBuffer.withSize(group.samplesThisCycle);

                            Resampling::resample(srcBuffer, interBuffer, srcToDestRatio,
                                                 p[0], p[1], p[2], p[3], p[4], p[5], p[6],
                                                 group.samplingSpillover[c],
                                                 resamplingAlgo);

                            destBuffer = group.cycleBuffer[c].write(interBuffer);
                        }
                    }

                    if (noteState.isStereo) {
                        destBuffer.mul(pans[c]);
                    }
                }

                if (!noteState.isStereo) {
                    Buffer<float> written = group.cycleBuffer[Right].write(destBuffer);

                    destBuffer.mul(pans[Left]);
                    written.mul(pans[Right]);
                }

                jassert(group.cycleBuffer[Left].writePosition == group.cycleBuffer[Right].writePosition);
                jassert(group.cycleBuffer[Left].readPosition == group.cycleBuffer[Right].readPosition);
            }

            jassert(group.cycleBuffer[Left].writePosition == group.cycleBuffer[Right].writePosition);
            jassert(group.cycleBuffer[Left].readPosition == group.cycleBuffer[Right].readPosition);

            minFrontier = jmin(minFrontier, group.sampledFrontier);
        }

        frame.frontier = minFrontier;

        layerAccumBuffer[Left].copyTo(pastCycle[Left]);
        layerAccumBuffer[Right].copyTo(pastCycle[Right]);
        // ippsCopy_32f(layerAccumBuffer[Left], pastCycle[Left], noteState.nextPow2);
        // ippsCopy_32f(layerAccumBuffer[Right], pastCycle[Right], noteState.nextPow2);

        // if we didn't have a cached first cycle
        //		if(updatePosition == 0)
        if (futureFrame.cycleCount == 0) {
            for (int j = 0; j < noteState.numUnisonVoices; ++j) {
                pastCycle[Left].copyTo(groups[j].lastLerpHalf[Left].withSize(noteState.nextPow2 / 2));
                pastCycle[Right].copyTo(groups[j].lastLerpHalf[Right].withSize(noteState.nextPow2 / 2));
                // ippsCopy_32f(pastCycle[Left], groups[j].lastLerpHalf[Left], noteState.nextPow2 / 2);
                // ippsCopy_32f(pastCycle[Right], groups[j].lastLerpHalf[Right], noteState.nextPow2 / 2);
            }
        }
    }

    //	if(updatePosition > lastSampleToRender)
    //	{
    //		updatePosition -= updateThreshSamples;
    ////		cout << "retracting update position " << updateThreshSamples << " samples to " << updatePosition << "\n";
    //		skipCalcNextPass = true;
    //	}
}

/**
 * Cycle buffer is saturated to buffer size + overflow
 * move overflow to start and set write point to end of overflow
 */
void CycleBasedVoice::commitCycleBufferAndWrap(StereoBuffer& channelPair) {
    int numSamples = channelPair.left.size();
    int srcChanCount = noteState.isStereo ? 2 : 1;
    int dstChanCount = channelPair.numChannels;

    for (int uniIdx = 0; uniIdx < noteState.numUnisonVoices; ++uniIdx) {
        VoiceParameterGroup& group = groups[uniIdx];

        if (!group.cycleBuffer[Left].hasDataFor(numSamples)) {
            return;
        }

        if (srcChanCount == dstChanCount) {
            for (int c = 0; c < dstChanCount; ++c) {
                Buffer<float> cycleBuf = group.cycleBuffer[c].read(numSamples);
                channelPair[c].add(cycleBuf);

                group.cycleBuffer[c].retract();
            }
        } else if (srcChanCount == 1) {
            Buffer<float> cycleBuf = group.cycleBuffer[Left].read(numSamples);
            group.cycleBuffer[Right].read(numSamples);
            group.cycleBuffer[Right].retract();

            channelPair.left.add(cycleBuf);
            channelPair.right.add(cycleBuf);

            group.cycleBuffer[Left].retract();
        } else if (dstChanCount == 1) {
            for (int c = 0; c < srcChanCount; ++c) {
                Buffer<float> cycleBuf = group.cycleBuffer[c].read(numSamples);
                channelPair.left.addProduct(cycleBuf, 0.5f);

                group.cycleBuffer[c].retract();
            }
        }
    }
}

void CycleBasedVoice::fillLatency(StereoBuffer& channelPair) {
    int numSamples = channelPair.left.size();

    rastBuffer.withSize(2 * numSamples).zero();

    if (cycleCompositeAlgo == Interpolate || parent->flags.haveFilter || parent->flags.haveFFTPhase) {
        for (int c = 0; c < 2; ++c) {
            Buffer sumBuffer(rastBuffer + c * numSamples, numSamples);

            Buffer<float> oscTail;
            if (oversamplers[c]->getLatencySamples() > 0) {
                oscTail = oversamplers[c]->getTail();
                jassert(oscTail.size() < sumBuffer.size());
            }

            if (resamplingAlgo == Resampling::Sinc) {
#ifdef USE_IPP

                // to be accurate we'd have to process the osc latency tail through the resampler and then get the tail...
                for (int i = 0; i < noteState.numUnisonVoices; ++i) {
                    VoiceParameterGroup& group = groups[i];
                    int start = 0;

                    if (oscTail.size() > 0) {
                        Buffer<float> output = group.resamplers[c].resample(oscTail);
                        sumBuffer.add(output);
                        start += output.size();
                    }

                    Buffer<float> resamplerTail = group.resamplers[c].finalise();
                    sumBuffer.offset(start)
                        .add(resamplerTail)
                        .withSize(jmin((int) resamplerTail.size(), numSamples - start));
                }
#endif
            } else {
                sumBuffer.add(oscTail);
            }

            channelPair[c].add(sumBuffer);
        }
    }
}

double CycleBasedVoice::getAngleDelta(int midiNumber, float detuneCents, double pitchEnvVal) {
    double pitchWheelSemis = parent ? parent->getPitchWheelValueSemitones() : 0;

    if (detuneCents == 0.f && pitchEnvVal == 0.5 && pitchWheelSemis == 0.) {
        return audioSource->angleDeltas[midiNumber - getConstant(LowestMidiNote)] / 44100.0;
    }

    double pitchEnvSemis = NumberUtils::unitPitchToSemis(pitchEnvVal);
    double fineTune = NumberUtils::noteToFrequency(midiNumber, detuneCents + (pitchEnvSemis + pitchWheelSemis) * 100);

    return fineTune / 44100.0;
}

void CycleBasedVoice::testIfOversamplingChanged() {
    int oldFactor = oversamplers[0]->getOversampleFactor();
    int factor = -1;

#if PLUGIN_MODE
    PluginProcessor& proc = getObj(PluginProcessor);

    factor = (proc.isNonRealtime()) ? getDocSetting(OversampleFactorRend) : getDocSetting(OversampleFactorRltm);
#else
    factor = getDocSetting(OversampleFactorRltm);
#endif

    jassert(factor >= 1 && factor <= 16);

    if (oldFactor == factor) {
        return;
    }

    bool subsample = getDocSetting(SubsampleRltm) || factor > 1;
    int maxBufferSize = getConstant(MaxCyclePeriod) * factor;
    int minLayerAccumSize = jmax(getConstant(MaxCyclePeriod) + 2, maxBufferSize);

    timeRasterizer.setIntegralSampling(subsample);

    for (int c = 0; c < 2; ++c) {
        layerAccumBuffer[c].resize(minLayerAccumSize);
        rastBuffer.resize(maxBufferSize);
        oversamplers[c]->setOversampleFactor(factor);
    }

    initCycleBuffers();
}

void CycleBasedVoice::stealNoteFrom(CycleBasedVoice* oldVoice) {
    vector<VoiceParameterGroup> groupCopy;

    initialiseNote(oldVoice->noteState.lastNoteNumber, parent->velocity);
    noteState.absVoiceTime = oldVoice->noteState.absVoiceTime;

    int oldGroupSize = (int) oldVoice->groups.size();

    for (int i = 0; i < (int) groups.size(); ++i)
        groups[i] = oldVoice->groups[jmin(oldGroupSize - 1, i)];
}

void CycleBasedVoice::ensureOversampleBufferSize(int numSamples) {
    if (parent == nullptr) {
        return;
    }

    Buffer<float> workBuffer = parent->audioSource->getWorkBuffer();
    int ovspNumSamples = oversamplers[0]->getOversampleFactor() * numSamples;

    if (workBuffer.size() < 2 * ovspNumSamples) {
        emergencyBuffer.ensureSize(ovspNumSamples * 2);

        oversampleAccumBuf.left = emergencyBuffer.withSize(ovspNumSamples);
        oversampleAccumBuf.right = Buffer(emergencyBuffer + ovspNumSamples, ovspNumSamples);
    } else {
        oversampleAccumBuf.left = workBuffer.withSize(ovspNumSamples);
        oversampleAccumBuf.right = (workBuffer + ovspNumSamples).withSize(ovspNumSamples);
    }
}

inline void CycleBasedVoice::updateChainAngleDelta(VoiceParameterGroup& group,
                                                   bool unisonEnabled, bool useFirstEnvelopeIndex) {
    if (parent != nullptr) {
        // possibly updated by parameter smoothing
        float unisonTune = unisonEnabled ? unison->getDetune(group.unisonIndex) : 0;
        double pitchEnvVal = 0.5;

        // TODO implement render graph
        EnvRastGroup& pitchGroup = parent->pitchGroup;
        EnvRasterizer& pitchRast = pitchGroup.envGroup.front().rast;
        MeshLibrary::EnvProps* props = parent->meshLib->getEnvProps(pitchGroup.layerGroup, 0);

        if (props != nullptr && props->active && pitchRast.hasEnoughCubesForCrossSection()) {
            int rastIndex = EnvRasterizer::headUnisonIndex + (useFirstEnvelopeIndex ? 0 : group.unisonIndex);
            float y = pitchRast.getSustainLevel(rastIndex);

            NumberUtils::constrain(y, 0.01f, 0.99f);
            pitchEnvVal = y;
        }

        group.angleDelta = getAngleDelta(noteState.lastNoteNumber, unisonTune, pitchEnvVal);
    }
}

inline void CycleBasedVoice::updateEnvelopes(int paramIndex, int deltaSamples) {
    for (int i = 0; i < parent->scratchGroup.size(); ++i) {
        parent->scratchGroup[i].scratchTime = noteState.absVoiceTime;
    }

    parent->updateSmoothedParameters(deltaSamples);

    for (int e = 0; e < numElementsInArray(parent->envGroups); ++e) {
        EnvRastGroup& group = *parent->envGroups[e];

        for (int i = 0; i < group.size(); ++i) {
            float tempoScale = 1.f; // TODO
            EnvRenderContext& renderRast = group[i];
            MeshLibrary::EnvProps* props = getObj(MeshLibrary).getEnvProps(group.layerGroup, renderRast.layerIndex);

            if (renderRast.sampleable && props->active) {
                renderRast.rast.renderToBuffer(deltaSamples, noteState.timePerOutputSample, paramIndex, *props,
                                               tempoScale);
                renderRast.scratchTime = jlimit(0.f, 1.f, renderRast.rast.getSustainLevel(paramIndex));
            }
        }
    }
}

void CycleBasedVoice::updateValue(int outputId, int dim, float value) {
    MeshLibrary& meshLib = *parent->meshLib;
    MeshLibrary::GroupLayerPair groupPair = getObj(ModMatrixPanel).toLayerIndex(outputId);

    if (groupPair.isNotNull()) {
        MeshLibrary::EnvProps& props = *meshLib.getEnvProps(groupPair);

        EnvRastGroup& group = groupPair.groupId == LayerGroups::GroupVolume
                                  ? parent->volumeGroup
                                  : groupPair.groupId == LayerGroups::GroupPitch
                                        ? parent->pitchGroup
                                        : parent->scratchGroup;

        if (group.envGroup.empty())
            return;

        int realIndex = CommonEnums::Null;
        for (int i = 0; i < group.size(); ++i) {
            if (group.envGroup[i].layerIndex == groupPair.layerIdx) {
                realIndex = i;
            }
        }

        bool valid = realIndex != CommonEnums::Null;

        if (!props.global && props.active && valid) {
            EnvRenderContext& renderRast = group.envGroup[realIndex];
            EnvRasterizer& envRast = renderRast.rast;

            envRast.updateValue(dim, value);

            if (props.dynamic) {
                envRast.calcCrossPoints();
                envRast.validateState();

                renderRast.sampleable = envRast.isSampleable();
            }
        }
    }
}

void CycleBasedVoice::unisonVoiceCountChanged() {
    // destructor deletes state objects so free them now to avoid double deletion
    // when vector resizes
#ifdef USE_IPP
    for (auto& group: groups) {
        group.resamplers[Left].freeState();
        group.resamplers[Right].freeState();
    }
#endif

    int maxPeriod = getConstant(MaxCyclePeriod);

    groups.resize(noteState.numUnisonVoices);
    lerpMemory.ensureSize(maxPeriod * noteState.numUnisonVoices);

    for (int i = 0; i < noteState.numUnisonVoices; ++i) {
        VoiceParameterGroup& group = groups[i];

        group.unisonIndex = i;
        group.lastLerpHalf[Left] = lerpMemory.place(maxPeriod / 2);
        group.lastLerpHalf[Right] = lerpMemory.place(maxPeriod / 2);
    }

    initCycleBuffers();
    testNumLayersChanged();
}

void CycleBasedVoice::testNumLayersChanged() {
    // TODO replace these locks with meshlibrary->arraylock

    ScopedLock sl(waveform3D->getLayerLock());

    for (int unisonIdx = 0; unisonIdx < noteState.numUnisonVoices; ++unisonIdx) {
        VoiceParameterGroup& group = groups[unisonIdx];

        int numLayers = timeLayers->size();

        if (group.layerStates.size() != numLayers)
            group.layerStates.resize(numLayers);
    }
}

void CycleBasedVoice::testIfResamplingQualityChanged() {
    bool isRealtime = true;

    onlyPlug(isRealtime = ! getObj(PluginProcessor).isNonRealtime());

    resamplingAlgo = isRealtime ? getDocSetting(ResamplingAlgoRltm) : getDocSetting(ResamplingAlgoRend);
}

float CycleBasedVoice::getScratchTime(int layerIndex, double cumePos) {
    if (layerIndex == CommonEnums::Null)
        return noteState.absVoiceTime;

    float blockOffset = double(parent->blockStartOffset - noteState.totalSamplesPlayed) + cumePos;
    MeshLibrary::EnvProps& props = *parent->meshLib->getEnvProps(LayerGroups::GroupScratch, layerIndex);

    if (!props.active) {
        return noteState.absVoiceTime;
    }

    if (props.global) {
        Buffer<float> scratchBuffer = audioSource->getScratchBuffer(layerIndex);
        jassert(isPositiveAndBelow((int) blockOffset, scratchBuffer.size()));

        return Resampling::lerpC(scratchBuffer, blockOffset);
    }

    for (auto& scratch: parent->scratchGroup.envGroup) {
        if (scratch.layerIndex == layerIndex) {
            return scratch.scratchTime;
        }
    }

    return 0;
}

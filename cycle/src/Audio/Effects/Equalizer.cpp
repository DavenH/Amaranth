#include <Obj/Ref.h>
#include <Algo/FFT.h>
#include <App/AppConstants.h>
#include <App/SingletonRepo.h>
#include "Equalizer.h"
#include "../../UI/Effects/GuilessEffect.h"

Equalizer::Equalizer(SingletonRepo *repo) : Effect(repo, "Equalizer")
                                            , samplerate(44100)
                                            , filterOrder(1) {
    double freqs[numPartitions] = {60, 250, 1200, 4000, 8000};
    Dsp::Cascade *cascadeArr[numPartitions] = {&lsFilter, &bsFilter[0], &bsFilter[1], &bsFilter[2], &hsFilter};

    for (int i = 0; i < numPartitions; ++i) {
        partitions[i].centreFreq.setValueDirect(freqs[i]);
        partitions[i].cascade = cascadeArr[i];
    }

    overflowBuffer.resize(16);
    clearGraphicBuffer();
    updateFilters();
}

Equalizer::~Equalizer() {
    freeStates();

    stateBuffer.clear();
    overflowBuffer.clear();
    delayBuffer.clear();
}

void Equalizer::freeStates() {
    ScopedLock sl(stateLock);

    for (auto& partition : partitions) {
        for (int c = 0; c < numEqChannels; ++c) {
            IppsIIRState64f_32f **state = &partition.states[c];

            if (*state != 0) {
                //				status(ippsIIRFree64f_32f(*state));
                *state = 0;
            }
        }
    }
}

void Equalizer::processBuffer(AudioSampleBuffer &buffer) {
    int numSamples = buffer.getNumSamples();

    ScopedLock sl(stateLock);

    for (int c = 0; c < jmin(2, buffer.getNumChannels()); ++c) {
        Buffer destBuffer(buffer.getWritePointer(c), numSamples);

        for (auto& part : partitions) {
            if (part.gainDB.getCurrentValue() == 0) {
                continue;
            }

            jassert(part.states[c] != nullptr);

            statusB(ippsIIR64f_32f(destBuffer, destBuffer, numSamples, part.states[c]));
        }
    }
}

void Equalizer::setSampleRate(double samplerate) {
    if (this->samplerate == samplerate) {
        return;
    }

    this->samplerate = samplerate;
    updateFilters();
}

bool Equalizer::doParamChange(int index, double value, bool doFurtherUpdate) {
    //	progressMark
    jassert(partitions[0].states[0] != nullptr);

    int partIdx = index % Band1Freq;
    bool didChange = false;

    if (index >= Band1Gain && index <= Band5Gain) {
        didChange = partitions[partIdx].gainDB.setTargetValue(calcGain(value));
    } else {
        didChange = partitions[partIdx].centreFreq.setTargetValue(calcFreq(value, getConstant(LogTension)));
    }

    if (doFurtherUpdate && didChange) {
        updatePartition(partIdx, true);
    }

    return true;
}


void Equalizer::updateFilters() {
    int tapsLength = 2 * tapsPerBiquad;
    int delayLineLength = (2 * filterOrder);

    bool haveResizedBuffers = false;
    haveResizedBuffers |= tapsBuffer.resize(numPartitions * tapsLength * numEqChannels);
    haveResizedBuffers |= delayBuffer.resize(numPartitions * delayLineLength * numEqChannels);

    tapsBuffer.zero();
    delayBuffer.zero();

    for (int i = 0; i < numPartitions; ++i) {
        updatePartition(i, !haveResizedBuffers);
    }

    if (haveResizedBuffers) {
        updateStates();

        /*
        for(int i = 0; i < numPartitions; ++i)
        {
            EqPartition& part = partitions[i];
            for(int c = 0; c < numEqChannels; ++c)
            {
                jassert(part.states[c]);
                ippsIIRSetTaps64f_32f(part.taps[c], part.states[c]);
            }
        }
        */
    }
}

void Equalizer::updatePartition(int idx, bool canUpdateTaps) {
    //	progressMark

    EqPartition& part = partitions[idx];
    int tapsLength = 2 * tapsPerBiquad;
    int delayLineLength = (2 * filterOrder);
    int tapsOffset = numEqChannels * tapsLength * idx;
    int delayOffset = numEqChannels * delayLineLength * idx;

    switch (idx) {
        case lowShelfPartition:
            lsFilter.setup(filterOrder, samplerate, part.centreFreq, part.gainDB);
            break;

        case highShelfPartition:
            hsFilter.setup(filterOrder, samplerate, part.centreFreq, part.gainDB);
            break;

        default:
            bsFilter[idx - 1].setup(filterOrder, samplerate, part.centreFreq, part.centreFreq * 0.7, part.gainDB);
            break;
    }

    for (int c = 0; c < numEqChannels; ++c) {
        tapsLength = part.cascade->getNumStages() * tapsPerBiquad;

        part.taps[c] = Buffer(tapsBuffer + tapsOffset + c * tapsLength, tapsLength);
        part.delayLine[c] = Buffer(delayBuffer + delayOffset + c * delayLineLength, delayLineLength);

        Buffer<double> &t = part.taps[c];
        int count = 0;

        for (int s = 0; s < part.cascade->getNumStages(); ++s) {
            const Dsp::Cascade::Stage &st = (*part.cascade)[s];
            const double taps[] = {st.m_b0, st.m_b1, st.m_b2, st.m_a0, st.m_a1, st.m_a2};

            ippsCopy_64f(taps, t + count, tapsPerBiquad);
            count += tapsPerBiquad;
        }

        //		if(part.states[c] != 0 && canUpdateTaps)
        //			ippsIIRSetTaps64f_32f(part.taps[c], part.states[c]);
    }
}

void Equalizer::processVertexBuffer(Buffer<Ipp32f> inputBuffer) {
    inputBuffer.section(inputBuffer.size() - overflowBuffer.size(),
                        overflowBuffer.size()).copyTo(overflowBuffer);

    for (auto& partition : partitions) {
        IppsIIRState64f_32f *state = partition.states[graphicEqChannel];

        ippsIIR64f_32f(overflowBuffer, overflowBuffer, overflowBuffer.size(), state);
        ippsIIR64f_32f(inputBuffer, inputBuffer, inputBuffer.size(), state);
    }

    ippsThreshold_GTAbs_32f_I(inputBuffer, inputBuffer.size(), 30.f);
}

void Equalizer::clearGraphicBuffer() {
    overflowBuffer.zero();
}

bool Equalizer::isEnabled() const {
    return ui->isEffectEnabled();
}

double Equalizer::calcFreq(double value, double logTension) {
    return jmax(40., 16000. * (exp(value * log(logTension + 1)) - 1) / logTension);
}

double Equalizer::calcKnobValue(double value, double logTension) {
    double x = value / 16000;
    return jlimit(0., 1., log(logTension * x + 1) / log(logTension + 1));
}

void Equalizer::updateStates() {
    ScopedLock sl(stateLock);

    freeStates();
    vector<int> sizes;
    int cumeSize = 0;

    for (auto& part : partitions) {
        for (int chan = 0; chan < numEqChannels; ++chan) {
            int stateSize;
            statusB(ippsIIRGetStateSize64f_BiQuad_32f(part.numCascades, &stateSize));

            stateSize *= 2;
            cumeSize += stateSize;
            sizes.push_back(stateSize);
        }
    }

    stateBuffer.resize(cumeSize);
    cumeSize = 0;
    int allocIndex = 0;

    for (auto& part : partitions) {
        for (int chan = 0; chan < numEqChannels; ++chan) {
            Buffer<Ipp8u> buffer = stateBuffer.section(cumeSize, sizes[allocIndex]);
            statusB(ippsIIRInit64f_BiQuad_32f(&part.states[chan], part.taps[chan],
                part.numCascades, part.delayLine[chan], buffer));

            cumeSize += sizes[allocIndex++];
        }
    }
}

void Equalizer::updateSmoothedParameters(int deltaSamples) {
    //	progressMark

    for (int i = 0; i < numPartitions; ++i) {
        EqPartition &part = partitions[i];

        part.centreFreq.update(deltaSamples);
        part.gainDB.update(deltaSamples);

        if (part.centreFreq.hasRamp() || part.gainDB.hasRamp())
            updatePartition(i, true);
    }
}

void Equalizer::updateParametersToTarget() {
    //	progressMark

    for (int i = 0; i < numPartitions; ++i) {
        EqPartition &part = partitions[i];

        part.centreFreq.updateToTarget();
        part.gainDB.updateToTarget();

        updatePartition(i, true);
    }
}

void Equalizer::test() {
    int size = 1024;
    int width = 16;
    int pulseWidth = 16;

    ScopedAlloc<Ipp32f> buffers(size);
    Transform fft;

    fft.allocate(size, true);

    AudioSampleBuffer audioBuffer(2, size);
    audioBuffer.clear();

    doParamChange(Band1Gain, 0.5001, false);
    doParamChange(Band2Gain, 0.5001, false);
    doParamChange(Band3Gain, 0.5001, false);
    doParamChange(Band4Gain, 0.5001, false);
    doParamChange(Band5Gain, 0.5001, true);

    doParamChange(Band1Freq, 0.1, false);
    doParamChange(Band2Freq, 0.3, false);
    doParamChange(Band3Freq, 0.5, false);
    doParamChange(Band4Freq, 0.7, false);
    doParamChange(Band5Freq, 0.9, true);

    updateParametersToTarget();

    for (int i = 0; i < numPartitions; ++i) {
        EqPartition &part = partitions[i];

        std::cout << "Eq partition " << String(i) << "\n";
        std::cout << "Gain: " << String(part.gainDB.getCurrentValue(), 2) << "\n";
        std::cout << "Freq: " << String(part.centreFreq.getCurrentValue(), 2) << "\n";

        //		for(int j = 0; j < 2; ++j)
        //		{
        //			std::cout << "State: " << part.states[j] << "\n";
        //			std::cout << ""
        //		}

        std::cout << "\n";
    }

    for (int i = 0; i < 2; ++i) {
        Buffer<float> buf(audioBuffer, i);

        //		Ipp32f* buffer = audioBuffer.getWritePointer(i);

        buf.zero();
        buf.section(pulseWidth, pulseWidth).set(1.f);
        buf.section(pulseWidth * 2, pulseWidth).set(-1.f);
        buf.copyTo(buffers);

        fft.forward(buffers);

        Arithmetic::applyLogMapping(fft.getMagnitudes(), (float) getConstant(FFTLogTensionAmp));
        // getObj(CsvFile).addValues(fft.getMagnitudes(), i);
    }

    processBuffer(audioBuffer);

    for (int i = 0; i < 2; ++i) {
        ippsCopy_32f(audioBuffer.getWritePointer(i), buffers, size);
        fft.forward(buffers);

        Arithmetic::applyLogMapping(fft.getMagnitudes(), (float) getConstant(FFTLogTensionAmp));
        // getObj(CsvFile).addValues(fft.getMagnitudes(), i + 2);
    }

    // getObj(CsvFile).print(repo);
}

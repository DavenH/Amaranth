#include <Obj/Ref.h>
#include <Algo/FFT.h>
#include <App/AppConstants.h>
#include <App/SingletonRepo.h>
#include "Equalizer.h"

#include <Util/StatusChecker.h>

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
    overflowBuffer.clear();
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

            part.iir.process(c, destBuffer);
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

    int partIdx = index % Band1Freq;
    bool didChange = false;

    if (index >= Band1Gain && index <= Band5Gain) {
        didChange = partitions[partIdx].gainDB.setTargetValue(calcGain(value));
    } else {
        didChange = partitions[partIdx].centreFreq.setTargetValue(calcFreq(value, getConstant(LogTension)));
    }

    if (doFurtherUpdate && didChange) {
        updatePartition(partIdx);
    }

    return true;
}


void Equalizer::updateFilters() {
    for (int i = 0; i < numPartitions; ++i) {
        updatePartition(i);
    }
}

void Equalizer::updatePartition(int idx) {
    //	progressMark

    EqPartition& part = partitions[idx];

    switch (idx) {
        case lowShelfPartition:
            lsFilter.setup(filterOrder, samplerate, part.centreFreq, part.gainDB);
            break;

        case highShelfPartition:
            hsFilter.setup(filterOrder, samplerate, part.centreFreq, part.gainDB);
            break;

        default:
            bsFilter[idx - 1].setup(filterOrder, samplerate, part.centreFreq, part.centreFreq * 0.7f, part.gainDB);
            break;
    }
    part.iir.updateFromCascade(*part.cascade);
}

void Equalizer::processVertexBuffer(Buffer<Float32> inputBuffer) {
    inputBuffer.section(inputBuffer.size() - overflowBuffer.size(),
                        overflowBuffer.size()).copyTo(overflowBuffer);

    for (auto& partition : partitions) {
        partition.iir.process(graphicEqChannel, overflowBuffer);
        partition.iir.process(graphicEqChannel, inputBuffer);
    }

    inputBuffer.clip(-30.f, 30.f);
}

void Equalizer::clearGraphicBuffer() {
    overflowBuffer.zero();

    for (auto& partition : partitions) {
        partition.iir.clear();
    }
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

void Equalizer::updateSmoothedParameters(int deltaSamples) {
    //	progressMark

    for (int i = 0; i < numPartitions; ++i) {
        EqPartition &part = partitions[i];

        part.centreFreq.update(deltaSamples);
        part.gainDB.update(deltaSamples);

        if (part.centreFreq.hasRamp() || part.gainDB.hasRamp()) {
            updatePartition(i);
        }
    }
}

void Equalizer::updateParametersToTarget() {
    //	progressMark

    for (int i = 0; i < numPartitions; ++i) {
        EqPartition &part = partitions[i];

        part.centreFreq.updateToTarget();
        part.gainDB.updateToTarget();

        updatePartition(i);
    }
}

// void Equalizer::test() {
//     int size = 1024;
//     int width = 16;
//     int pulseWidth = 16;
//
//     ScopedAlloc<Float32> buffers(size);
//     Transform fft;
//
//     fft.allocate(size, true);
//
//     AudioSampleBuffer audioBuffer(2, size);
//     audioBuffer.clear();
//
//     doParamChange(Band1Gain, 0.5001, false);
//     doParamChange(Band2Gain, 0.5001, false);
//     doParamChange(Band3Gain, 0.5001, false);
//     doParamChange(Band4Gain, 0.5001, false);
//     doParamChange(Band5Gain, 0.5001, true);
//
//     doParamChange(Band1Freq, 0.1, false);
//     doParamChange(Band2Freq, 0.3, false);
//     doParamChange(Band3Freq, 0.5, false);
//     doParamChange(Band4Freq, 0.7, false);
//     doParamChange(Band5Freq, 0.9, true);
//
//     updateParametersToTarget();
//
//     for (int i = 0; i < numPartitions; ++i) {
//         EqPartition &part = partitions[i];
//
//         info("Eq partition " << String(i) << "\n");
//         info("Gain: " << String(part.gainDB.getCurrentValue(), 2) << "\n");
//         info("Freq: " << String(part.centreFreq.getCurrentValue(), 2) << "\n");
//
//         //		for(int j = 0; j < 2; ++j)
//         //		{
//         //			info("State: " << part.states[j] << "\n");
//         //			std::cout << ""
//         //		}
//
//         info("\n");
//     }
//
//     for (int i = 0; i < 2; ++i) {
//         Buffer<float> buf(audioBuffer, i);
//
//         //		Float32* buffer = audioBuffer.getWritePointer(i);
//
//         buf.zero();
//         buf.section(pulseWidth, pulseWidth).set(1.f);
//         buf.section(pulseWidth * 2, pulseWidth).set(-1.f);
//         buf.copyTo(buffers);
//
//         fft.forward(buffers);
//
//         Arithmetic::applyLogMapping(fft.getMagnitudes(), (float) getConstant(FFTLogTensionAmp));
//         // getObj(CsvFile).addValues(fft.getMagnitudes(), i);
//     }
//
//     processBuffer(audioBuffer);
//
//     for (int i = 0; i < 2; ++i) {
//         ippsCopy_32f(audioBuffer.getWritePointer(i), buffers, size);
//         fft.forward(buffers);
//
//         Arithmetic::applyLogMapping(fft.getMagnitudes(), (float) getConstant(FFTLogTensionAmp));
//         // getObj(CsvFile).addValues(fft.getMagnitudes(), i + 2);
//     }
//
//     // getObj(CsvFile).print(repo);
// }

#include "RealTimePitchTracker.h"

#include <Array/ScopedAlloc.h>

#include <limits>

#include "PitchKernelBuilder.h"

using std::vector;

RealTimePitchTracker::RealTimePitchTracker(Algorithm algo) :
        RealTimePitchTracker(PitchTrackingRequest{}, algo) {
}

RealTimePitchTracker::RealTimePitchTracker(const PitchTrackingRequest& request, Algorithm algo) :
        algorithm   (algo)
    ,   request     (request)
    ,   rwBuffer    (blockSize + 4096) {
    localBlock.resize(blockSize);
    localBlock.zero();
    hannWindow.resize(blockSize);
    hannWindow.hann();

    if (algo == AlgoSpectral) {
        kernelMemory.resize(request.numMidiNotes * blockSize);
        fftFreqs.resize(blockSize / 2);
        fftMagnitudes.resize(blockSize / 2);
        correlations.resize(request.numMidiNotes);
    } else {
        rawBlock.resize(blockSize);
        rawBlock.zero();
        periodScores.resize(request.numMidiNotes);
        cycleDiffScratch.resize(blockSize);
        tauTable.resize(request.numMidiNotes);
    }
}

void RealTimePitchTracker::setSampleRate(int sr) {
    sampleRate = sr;

    if (algorithm == AlgoSpectral) {
        float delta = 1.f / blockSize;
        fftFreqs.ramp(delta, delta).mul((float) sr);
        transform.allocate(blockSize, Transform::NoDivByAny, true);
        createKernels(request.frequencyOfA4);
    } else {
        precomputePeriods(request.frequencyOfA4, sr);
    }
}

void RealTimePitchTracker::write(Buffer<float>& audioBlock) {
    if (!rwBuffer.hasRoomFor(audioBlock.size())) {
        rwBuffer.retract();
    }

    rwBuffer.write(audioBlock);

    if (rwBuffer.hasDataFor(blockSize)) {
        auto block = rwBuffer.read(blockSize);
        const SpinLock::ScopedTryLockType lock(bufferLock);
        if (lock.isLocked()) {
            if (algorithm == AlgoSpectral) {
                block.copyTo(localBlock);
                localBlock.mul(hannWindow);
                localBlock.mul(1 / jmax(0.01f, localBlock.max()));
            } else {
                block.copyTo(rawBlock);
                rawBlock.mul(1.f / jmax(0.01f, rawBlock.max()));
            }
        }
    }
}

int RealTimePitchTracker::update() {
    return algorithm == AlgoSpectral ? updateSpectral() : updatePeriodic();
}

int RealTimePitchTracker::updateSpectral() {
    if (kernels.empty()) {
        return bestKeyIndex;
    }

    {
        const SpinLock::ScopedLockType lock(bufferLock);
        transform.forward(localBlock);
    }

    Buffer<float> magnitudes = transform.getMagnitudes().add(0.5).ln();
    magnitudes.copyTo(fftMagnitudes);
    fftMagnitudes.sub(fftMagnitudes.mean());

    const float spectrumNorm = fftMagnitudes.normL2();
    fftMagnitudes.mul(spectrumNorm > 1.0e-8f ? 1.0f / spectrumNorm : 0.f);

    for (int i = 0; i < (int) kernels.size(); ++i) {
        correlations[i] = fftMagnitudes.dot(kernels[i]);
    }

    float maxCorrelation = -std::numeric_limits<float>::infinity();
    int bestKernelIndex = jlimit(0, request.numMidiNotes - 1, bestKeyIndex - request.firstMidiNote);
    correlations.getMax(maxCorrelation, bestKernelIndex);

    if (maxCorrelation > 0.1f) {
        bestKeyIndex = bestKernelIndex + request.firstMidiNote;
    }

    {
        const SpinLock::ScopedLockType lock(bufferLock);
        traceListener->onPitchFrame(bestKeyIndex, localBlock, correlations, fftMagnitudes);
    }
    return bestKeyIndex;
}

void RealTimePitchTracker::precomputePeriods(double frequencyOfA4, int sr) {
    tauTable.resize(request.numMidiNotes);
    for (int i = 0; i < request.numMidiNotes; ++i) {
        const float freq = MidiMessage::getMidiNoteInHertz(i + request.firstMidiNote, frequencyOfA4);
        tauTable[i] = jmax(2, roundToInt((float) sr / freq));
    }
}

int RealTimePitchTracker::updatePeriodic() {
    if (tauTable.empty()) {
        return bestKeyIndex;
    }

    {
        const SpinLock::ScopedLockType lock(bufferLock);
        if (rawBlock.normL2() < 1.f) {
            return bestKeyIndex;
        }
    }

    float* sig = rawBlock.get();

    for (int key = 0; key < request.numMidiNotes; ++key) {
        const int tau           = tauTable[key];
        const int numCycles     = blockSize / tau;

        if (numCycles < 2) {
            periodScores[key] = std::numeric_limits<float>::infinity();
            continue;
        }

        const int numComparisons = numCycles - 1;
        const int diffLen        = numComparisons * tau;

        Buffer<float> base   (sig,       diffLen);
        Buffer<float> lagged (sig + tau, diffLen);
        Buffer<float> scratch(cycleDiffScratch.get(), diffLen);
        VecOps::sub(lagged, base, scratch);

        const float rawScore = scratch.normL2();

        periodScores[key] = rawScore / (sqrtf((float) tau) * (float) numComparisons);
    }

    constexpr float octaveTolerance = 1.08f;

    for (int key = 0; key + 12 < request.numMidiNotes; ++key) {
        if (periodScores[key] == std::numeric_limits<float>::infinity()) {
            continue;
        }
        if (periodScores[key + 12] == std::numeric_limits<float>::infinity()) {
            continue;
        }

        if (periodScores[key + 12] <= periodScores[key] * octaveTolerance) {
            periodScores[key] *= 2.f;
        }
    }

    float minScore = std::numeric_limits<float>::infinity();
    int bestKey = jlimit(0, request.numMidiNotes - 1, bestKeyIndex - request.firstMidiNote);

    for (int key = 0; key < request.numMidiNotes; ++key) {
        if (periodScores[key] < minScore) {
            minScore = periodScores[key];
            bestKey  = key;
        }
    }

    constexpr float periodicityThreshold = 0.25f;

    if (minScore < periodicityThreshold) {
        bestKeyIndex = bestKey + request.firstMidiNote;
    }

    {
        const SpinLock::ScopedLockType lock(bufferLock);
        Buffer<float> emptyMags;
        traceListener->onPitchFrame(bestKeyIndex, rawBlock, periodScores, emptyMags);
    }

    return bestKeyIndex;
}

void RealTimePitchTracker::createKernels(double frequencyOfA4) {
    kernels.clear();
    kernelMemory.resetPlacement();

    const float topFrequency = 5000;
    const int numFreqs = fftFreqs.size();

    ScopedAlloc<float> invRamp(numFreqs);
    ScopedAlloc<float> strongHighPassWeight(numFreqs);
    invRamp.ramp(1, 0.08).inv();

    fftFreqs.copyTo(strongHighPassWeight);
    strongHighPassWeight.add(60.0f).divCRev(60.0f).subCRev(1.0f).clip(0.0f, 1.0f);
    strongHighPassWeight.mul(invRamp);

    for (int i = 0; i < request.numMidiNotes; ++i) {
        const int noteNumber = i + request.firstMidiNote;
        float candFreq = MidiMessage::getMidiNoteInHertz(noteNumber, frequencyOfA4);

        Buffer<float> kernel = kernelMemory.place(numFreqs);
        int numHarmonics = roundToInt(topFrequency / candFreq);

        ScopedAlloc<float> buff(numFreqs * 2);
        Buffer<float> q = buff.place(numFreqs);
        Buffer<float> a = buff.place(numFreqs);

        VecOps::mul(fftFreqs, 1 / candFreq, q);
        kernel.zero();

        PitchKernelBuilder::paintPrimeHarmonics(kernel, q, a, numHarmonics);

        kernel.mul(invRamp);
        const float normL2 = kernel.normL2();
        kernel.mul(MathConstants<float>::sqrt2 / jmax(1e-5f, normL2));
        kernels.push_back(kernel);
    }
}

void RealTimePitchTracker::setTraceListener(RealTimePitchTraceListener& listener) {
    traceListener = &listener;
}

void RealTimePitchTracker::useDefaultTraceListener() {
    traceListener = &defaultTraceListener;
}

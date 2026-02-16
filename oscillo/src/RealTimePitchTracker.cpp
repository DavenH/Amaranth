#include "RealTimePitchTracker.h"
#include <Array/ScopedAlloc.h>
#include <cmath>
#include <limits>

using std::vector;

RealTimePitchTracker::RealTimePitchTracker():
        kernelMemory(numKeys * blockSize)
    ,   rwBuffer(blockSize + 4096)
{
    fftFreqs.resize(blockSize / 2);
    fftMagnitudes.resize(blockSize / 2);
    correlations.resize(numKeys);
    localBlock.resize(blockSize);
    hannWindow.resize(blockSize);
    hannWindow.hann();
    localBlock.zero();
    fftMagnitudes.zero();
}

void RealTimePitchTracker::setSampleRate(int samplerate) {
    float delta = 1.f / blockSize;
    fftFreqs.ramp(delta, delta).mul((float) samplerate);
    transform.allocate(blockSize, Transform::NoDivByAny, true);
    createKernels(440.0);
}

void RealTimePitchTracker::write(Buffer<float>& audioBlock) {
    if (!rwBuffer.hasRoomFor(audioBlock.size())) {
        rwBuffer.retract();
    }
    rwBuffer.write(audioBlock);

    if (rwBuffer.hasDataFor(blockSize)) {
        auto block = rwBuffer.read(blockSize);
        {
            // just try to lock - audio thread updates more often, it's okay to queue up a few samples
            const SpinLock::ScopedTryLockType lock(bufferLock);
            if (lock.isLocked()) {
                block.copyTo(localBlock);
                localBlock.mul(hannWindow);
                localBlock.mul(1 / jmax(0.01f, localBlock.max()));
            }
        }
    }
}

int RealTimePitchTracker::update() {
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

    for (int i = 0; i < kernels.size(); ++i) {
        const auto& kernel = kernels[i];
        correlations[i] = fftMagnitudes.dot(kernel);
    }

    float maxCorrelation = -std::numeric_limits<float>::infinity();
    int bestKernelIndex = jlimit(0, numKeys - 1, bestKeyIndex - 21);
    correlations.getMax(maxCorrelation, bestKernelIndex);

    float spectralPeakLogMagnitude = -std::numeric_limits<float>::infinity();
    int spectralPeakIndex = 0;
    fftMagnitudes.getMax(spectralPeakLogMagnitude, spectralPeakIndex);

    if (maxCorrelation > 0.1f) {
        int midiNote = bestKernelIndex + 21; // Lowest key is A0 (MIDI note 21)
        bestKeyIndex = midiNote;
    }

    {
        const SpinLock::ScopedLockType lock(bufferLock);
        traceListener->onPitchFrame(bestKeyIndex, localBlock, correlations, fftMagnitudes);
    }
    return bestKeyIndex;
}

void RealTimePitchTracker::createKernels(double frequencyOfA4) {
    int primes[] = {   1,    2,    3,    5,    7,   11,   13,   17,   19,   23,   29,   31,   37,   41,   43,   47,   53,
                      59,   61,   67,   71,   73,   79,   83,   89,   97,  101,  103,  107,  109,  113,  127,  131,  137,
                     139,  149,  151,  157,  163,  167,  173,  179,  181,  191,  193,  197,  199,  211,  223,  227,  229,
                     233,  239,  241,  251,  257,  263,  269,  271,  277,  281,  283,  293,  307,  311,  313,  317,  331,
                     337,  347,  349,  353,  359,  367,  373,  379,  383,  389,  397,  401,  409,  419,  421,  431,  433,
                     439,  443,  449,  457,  461,  463,  467,  479,  487,  491,  499,  503,  509,  521,  523,  541,  547,
                     557,  563,  569,  571,  577,  587,  593,  599,  601,  607,  613,  617,  619,  631,  641,  643,  647 };

    kernels.clear();
    kernelMemory.resetPlacement();

    const float twoPi = MathConstants<float>::twoPi;
    const float topFrequency = 5000;
    const int numFreqs = fftFreqs.size();

    ScopedAlloc<float> invRamp(numFreqs);
    ScopedAlloc<float> strongHighPassWeight(numFreqs);
    invRamp.ramp(1, 0.08).inv();

    // Extra low-note tilt: w(f) = f / (f + f0)
    fftFreqs.copyTo(strongHighPassWeight);
    strongHighPassWeight.add(60.0f).divCRev(60.0f).subCRev(1.0f).clip(0.0f, 1.0f);
    strongHighPassWeight.mul(invRamp);

    std::cout << std::fixed << std::setprecision(3);
    for (int i = 0; i < strongHighPassWeight.size(); ++i) {
        std::cout << i << "\t" << strongHighPassWeight[i] << std::endl;
    }

    for (int i = 0; i < numKeys; ++i) {

        const int noteNumber = i + 21;
        float candFreq = MidiMessage::getMidiNoteInHertz(noteNumber, frequencyOfA4);

        Buffer<float> kernel = kernelMemory.place(numFreqs);
        int numHarmonics = roundToInt(topFrequency / candFreq);

        ScopedAlloc<float> buff(numFreqs * 2);
        Buffer<float> q = buff.place(numFreqs);
        Buffer<float> a = buff.place(numFreqs);

        VecOps::mul(fftFreqs, 1 / candFreq, q);
        kernel.zero();

        int primeIdx = 0;
        int startIdxA = 0, startIdxB = 0;

        while (primes[primeIdx] < numHarmonics) {
            int prime = primes[primeIdx];

            q.copyTo(a);
            a.sub((float) prime).abs();

            for (int k = startIdxA; k < numFreqs; ++k) {
                if (a[k] < 0.25f) {
                    startIdxA = k;

                    while (a[k] < 0.25f) {
                        kernel[k] = cosf(twoPi * a[k]);
                        ++k;
                    }
                    break;
                }
            }

            bool firstTrough = true;
            for (int k = startIdxB; k < numFreqs; ++k) {
                if (a[k] >= 0.25f && a[k] < 0.75f) {
                    startIdxB = k;

                    while (a[k] >= 0.25f && a[k] < 0.75f) {
                        kernel[k] += cosf(twoPi * q[k]) / 2;
                        ++k;
                    }

                    if (!firstTrough) {
                        break;
                    }

                    firstTrough = false;
                }
            }

            ++primeIdx;
        }

        kernel.mul(invRamp);
        // float highpassScale = (200.f - candFreq) / 200.f;
        // if (highpassScale > 0.f) {
        //     kernel.mul(1 - highpassScale);
        //     kernel.addProduct(strongHighPassWeight, highpassScale);
        // }
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

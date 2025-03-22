#include "RealTimePitchTracker.h"

#include <Array/ScopedAlloc.h>
using std::vector;

RealTimePitchTracker::RealTimePitchTracker():
        kernelMemory(numKeys * blockSize)
    ,   rwBuffer(blockSize + 2048)
{
    fftFreqs.resize(blockSize / 2);
    localBlock.resize(blockSize);
    hannWindow.resize(blockSize);
    hannWindow.hann();
    localBlock.zero();
}

void RealTimePitchTracker::setSampleRate(int samplerate) {
    float delta = 1.f / blockSize;
    fftFreqs.ramp(delta, delta).mul(samplerate / 2);
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
            const SpinLock::ScopedLockType lock(bufferLock);
            block.copyTo(localBlock);
            localBlock.mul(hannWindow);
        }
    }
}

pair<int, float> RealTimePitchTracker::update() {
    if (kernels.empty()) {
        return {bestKeyIndex, bestPitch};
    }

    {
        const SpinLock::ScopedLockType lock(bufferLock);
        Buffer<float> windowedBlock = localBlock;
        transform.forward(windowedBlock);
    }

    Buffer<float> magnitudes = transform.getMagnitudes();

    float maxCorrelation = 0.0f;
    int bestKey = bestKeyIndex; // Start with previous best key as default

    for (int i = 0; i < kernels.size(); ++i) {
        const auto& kernel = kernels[i];

        float correlation = magnitudes.dot(kernel);

        if (correlation > maxCorrelation) {
            maxCorrelation = correlation;
            bestKey = i;
        }
    }

    if (maxCorrelation > 0.1f) {
        int midiNote = bestKey + 21; // Assuming lowest key is A0 (MIDI note 21)

        // Calculate frequency for this MIDI note (we could refine this with quadratic interpolation)
        float pitch = MidiMessage::getMidiNoteInHertz(midiNote);

        {
            const SpinLock::ScopedLockType lock(bufferLock);
            bestKeyIndex = bestKey;
            bestPitch = pitch;
        }
    }

    return {bestKeyIndex, bestPitch};
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

        kernel.mul(MathConstants<float>::sqrt2 / kernel.normL2());
        kernels.push_back(kernel);
    }
}

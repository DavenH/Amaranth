#include "RealTimePitchTracker.h"
#include <Array/ScopedAlloc.h>
#include <cmath>
#include <iomanip>
#include <limits>

using std::vector;

RealTimePitchTracker::RealTimePitchTracker(Algorithm algo) :
        rwBuffer(blockSize + 4096),
        algorithm(algo)
{
    // Shared buffers needed by both modes
    localBlock.resize(blockSize);
    localBlock.zero();
    hannWindow.resize(blockSize);
    hannWindow.hann();

    if (algo == AlgoSpectral) {
        kernelMemory.resize(numKeys * blockSize);
        fftFreqs.resize(blockSize / 2);
        fftMagnitudes.resize(blockSize / 2);
        correlations.resize(numKeys);
    } else {
        rawBlock.resize(blockSize);
        rawBlock.zero();
        periodScores.resize(numKeys);
        cycleDiffScratch.resize(blockSize);
        tauTable.resize(numKeys);
    }
}

void RealTimePitchTracker::setSampleRate(int sr) {
    sampleRate = sr;

    if (algorithm == AlgoSpectral) {
        float delta = 1.f / blockSize;
        fftFreqs.ramp(delta, delta).mul((float) sr);
        transform.allocate(blockSize, Transform::NoDivByAny, true);
        createKernels(440.0);
    } else {
        precomputePeriods(440.0, sr);
    }
}

void RealTimePitchTracker::write(Buffer<float>& audioBlock) {
    if (!rwBuffer.hasRoomFor(audioBlock.size()))
        rwBuffer.retract();

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
                // No window: a Hann taper drives edge rows toward zero and
                // inflates cycle-diff scores even at the true period.
                block.copyTo(rawBlock);
                rawBlock.mul(1.f / jmax(0.01f, rawBlock.max()));
            }
        }
    }
}

int RealTimePitchTracker::update() {
    return algorithm == AlgoSpectral ? updateSpectral() : updatePeriodic();
}

// ─────────────────────────────────────────────────────────────────────────────
// Spectral mode: FFT log-magnitude correlation against harmonic kernels
// ─────────────────────────────────────────────────────────────────────────────

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

    for (int i = 0; i < (int) kernels.size(); ++i)
        correlations[i] = fftMagnitudes.dot(kernels[i]);

    float maxCorrelation = -std::numeric_limits<float>::infinity();
    int bestKernelIndex = jlimit(0, numKeys - 1, bestKeyIndex - 21);
    correlations.getMax(maxCorrelation, bestKernelIndex);

    if (maxCorrelation > 0.1f) {
        bestKeyIndex = bestKernelIndex + 21;
    }

    {
        const SpinLock::ScopedLockType lock(bufferLock);
        traceListener->onPitchFrame(bestKeyIndex, localBlock, correlations, fftMagnitudes);
    }
    return bestKeyIndex;
}

// ─────────────────────────────────────────────────────────────────────────────
// CycleDiff mode: cycle-to-cycle auto-difference in the time domain
// ─────────────────────────────────────────────────────────────────────────────

void RealTimePitchTracker::precomputePeriods(double frequencyOfA4, int sr) {
    tauTable.resize(numKeys);
    tauInvSqrtTable.resize(numKeys);
    tauLagCompTable.resize(numKeys);
    for (int i = 0; i < numKeys; ++i) {
        const float freq = MidiMessage::getMidiNoteInHertz(i + 21, frequencyOfA4);
        const int tau = jmax(2, roundToInt((float) sr / freq));
        const int numCycles = blockSize / tau;
        const int numComparisons = numCycles - 1;
        const int diffLen = numComparisons * tau;
        tauTable[i] = tau;
        tauInvSqrtTable[i] = diffLen > 0 ? 1.0f / std::sqrt((float) diffLen) : 0.0f;
        tauLagCompTable[i] = 1.0f / std::sqrt(std::sqrt((float) tau));
    }
}

int RealTimePitchTracker::updatePeriodic() {
    if (tauTable.empty()) {
        return bestKeyIndex;
    }
    ++periodicUpdateCount;

    // Silence guard: a peak-normalised non-silent block always has normL2 >> 1.
    // An all-zero block scores 0 on every candidate and would give a false reading.
    {
        const SpinLock::ScopedLockType lock(bufferLock);
        if (rawBlock.normL2() < 1.f) {
            return bestKeyIndex;
        }
    }

    float* sig = rawBlock.get();

    // ── 1. Score every candidate period ──────────────────────────────────────
    //
    // For candidate period τ, reshape the block into rows of width τ.
    // The sum of consecutive row-pair L2 differences equals:
    //
    //   Σ_{i=0}^{(numCycles-1)·τ - 1}  (x[i+τ] - x[i])²   (then sqrt'd)
    //
    // because when i sweeps [0,τ) it covers row 0 vs 1, [τ,2τ) covers row 1
    // vs 2, etc.  This collapses the inner per-cycle loop into two BLAS calls:
    // one sub and one normL2 over the full diffLen span.
    //
    // Normalize by sqrt(diffLen), where diffLen = numComparisons * tau,
    // then apply a mild short-lag compensation term. This counteracts the
    // tendency of very small lags to look artificially good on smooth signals.

    for (int key = 0; key < numKeys; ++key) {
        const int tau           = tauTable[key];
        const int numCycles     = blockSize / tau;

        if (numCycles < 2) {
            // Fewer than 2 complete cycles — insufficient data.
            // Affects roughly the bottom octave at 44100 Hz / 4096-sample block.
            periodScores[key] = std::numeric_limits<float>::infinity();
            continue;
        }

        const int numComparisons = numCycles - 1;
        const int diffLen        = numComparisons * tau;  // == numCycles*tau - tau <= blockSize

        // x[i+τ] - x[i] for i in [0, diffLen): one BLAS subtract
        Buffer<float> base   (sig,       diffLen);
        Buffer<float> lagged (sig + tau, diffLen);
        Buffer<float> scratch(cycleDiffScratch.get(), diffLen);
        VecOps::sub(lagged, base, scratch);

        // Single L2 norm over the full lagged-diff span: replaces the per-cycle loop
        const float rawScore = scratch.normL2();

        periodScores[key] = rawScore * tauInvSqrtTable[key] * tauLagCompTable[key];
    }

    if (periodicUpdateCount % 10 == 0) {

        // const std::streamsize oldPrecision = std::cout.precision();
        // const std::ios::fmtflags oldFlags = std::cout.flags();
        // std::cout << std::fixed << std::setprecision(3);
        // std::cout << "period_scores_frame," << periodicUpdateCount;
        // for (int key = 0; key < numKeys; ++key) {
        //     std::cout << "," << periodScores[key];
        // }
        // std::cout << std::endl;
        // std::cout.flags(oldFlags);
        // std::cout.precision(oldPrecision);
    }

    // ── 2. Winner + subharmonic suppression ──────────────────────────────────
    //
    // For a tone with period τ0, larger periods 2τ0, 3τ0... can also produce low
    // cycle-diff scores. This creates octave-down errors (e.g. A3 -> A2).
    //
    // After finding the global minimum, search only upward by octaves for the
    // same pitch class and take the highest octave still close to the minimum.
    // This keeps true pitch classes while suppressing subharmonic picks.

    float minScore = std::numeric_limits<float>::infinity();
    int   bestKey  = jlimit(0, numKeys - 1, bestKeyIndex - 21);

    for (int key = 0; key < numKeys; ++key) {
        if (periodScores[key] < minScore) {
            minScore = periodScores[key];
            bestKey  = key;
        }
    }

    const float octaveAcceptThreshold = jmax(0.08f, minScore * 2.6f);
    int octaveResolvedKey = bestKey;
    for (int key = bestKey + 12; key < numKeys; key += 12) {
        if (periodScores[key] <= octaveAcceptThreshold) {
            octaveResolvedKey = key;
        }
    }

    // Commit when either:
    // 1) the frame is clearly periodic in absolute terms, or
    // 2) the new candidate is substantially better than the currently latched key.
    //
    // This prevents stale latch lock-in (for example, a transient jump to a bad
    // key that never clears because later voiced frames miss the absolute gate).
    constexpr float periodicityThreshold = 0.25f;
    // const int prevKey = jlimit(0, numKeys - 1, bestKeyIndex - 21);
    // const float prevScore = periodScores[prevKey];
    // const bool clearlyBetterThanLatched = std::isfinite(prevScore) && (minScore * 1.15f < prevScore);
    //
    if (minScore < periodicityThreshold) {
        bestKeyIndex = octaveResolvedKey + 21;

        // Pass periodScores in place of correlations so the trace listener can
        // visualise the score landscape. Lower = stronger pitch candidate.
        {
            const SpinLock::ScopedLockType lock(bufferLock);
            Buffer<float> emptyMags;
            traceListener->onPitchFrame(bestKeyIndex, rawBlock, periodScores, emptyMags);
        }
    }
    return bestKeyIndex;
}

// ─────────────────────────────────────────────────────────────────────────────
// Kernel construction (spectral mode only)
// ─────────────────────────────────────────────────────────────────────────────

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

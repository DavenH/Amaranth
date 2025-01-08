#include "PitchTracker.h"

#include <Util/Arithmetic.h>

#include "Resampling.h"

#include "../Algo/FFT.h"
#include "../Audio/PitchedSample.h"

#ifdef BUILD_TESTING
  #define DEBUG_RENDERING
#endif
#include <Array/VecOps.h>

#include "../../tests/TestDefs.h"
#include "../../tests/SignalDebugger.h"

PitchTracker::PitchTracker() :
        algo(AlgoAuto)
    ,   aperiodicityThresh(0.3f) {
    reset();
}

void PitchTracker::yin() {
    if(sample == nullptr) {
        return;
    }

    data.maxFrequency        = 1500;
    data.octaveRatioThres    = 0.5;
    const int downsampleRate = 16000;

    float rateRatio   = float(downsampleRate) / sample->samplerate;
    int numSamples16k = rateRatio * sample->size();
    int length        = numSamples16k - data.offsetSamples;
    int maxlag        = (downsampleRate / data.minFrequency);
    data.step         = int(maxlag * 1.25f + jmax(0, (length - downsampleRate * 5) / 20));
    int inc           = data.step / data.overlap;
    int minlag        = downsampleRate / data.maxFrequency;
    int lagSize       = maxlag - minlag;
    int offset        = 0;

    ScopedAlloc<float> memory(sample->size() + numSamples16k + data.step + lagSize * 2);

    Buffer<float> wavBuff   = sample->audio.left;
    Buffer<float> wavCopy   = memory.place(sample->size());
    Buffer<float> resamp16k = memory.place(numSamples16k);
    Buffer<float> diff      = memory.place(data.step);
    Buffer<float> ramp      = memory.place(lagSize);
    Buffer<float> norms     = memory.place(lagSize);

    ramp.ramp(0, 0.3f / float(lagSize));

    {
        wavBuff.copyTo(wavCopy);

        ScopedAlloc<Float32> mem(64);
        Buffer<Float32> kernel = mem.place(mem.size() / 2);
        Buffer<Float32> window = mem.place(mem.size() / 2);
        VecOps::sinc(kernel, window, jmin(0.5f, 0.25f / rateRatio));
        // doesn't retract the kernel size...
        VecOps::conv(wavBuff, kernel, wavCopy);
    }

    Resampling::linResample(wavBuff, resamp16k);

    sample->resetPeriods();

    while (offset + data.step + maxlag < length) {
        norms.zero();

        for (int lag = minlag; lag < maxlag; ++lag) {
            int remaining = numSamples16k - (offset + lag);
            if (remaining > 0) {
                Buffer<float> d = diff.withSize(jmin(remaining, data.step));

                VecOps::sub(resamp16k + offset, resamp16k + lag + offset, d);
                norms[lag - minlag] += d.normL2();
            }
        }

        norms.mul(lagSize / norms.normL1()).add(ramp);

        int troughIndex    = getTrough(norms, minlag);
        float scaledPeriod = (float) (troughIndex + minlag) / rateRatio;
        float confidence   = norms[troughIndex];

        PitchFrame frame((int) ((float) offset / rateRatio), scaledPeriod, confidence);

        sample->addFrame(frame);

        offset += inc;
    }

    fillFrequencyBins();
    confidence = refineFrames(sample, getAveragePeriod());
}

float PitchTracker::refineFrames(PitchedSample* sample, float averagePeriod) {
    if(sample->periods.empty()) {
        return 0;
    }

    Buffer audio(sample->audio.left);

    int refineIters = 30;
    int numSamples  = audio.size();
    int lookahead   = jmax(2, roundToInt(2048.f / averagePeriod));

    float spread    = 0.0015;
    float lowRatio  = 1 - float(refineIters) * spread * 0.5f;

    ScopedAlloc<float> subnorms(refineIters);
    ScopedAlloc<float> memory(4096 * 3);
    Buffer diffBuff(memory.place(4096));
    Buffer waveBuff(memory.place(4096));
    Buffer offsetBuff(memory.place(4096));

    float currentPeriod = sample->periods.front().period;
    float bestNorm      = 1000;
    float cumeBest      = 0;

    for (auto & frame : sample->periods) {
        int offset = frame.sampleOffset;

        if (frame.sampleOffset >= audio.size()) {
            continue;
        }

        subnorms.zero();

        if (audio.sectionAtMost(frame.sampleOffset, int(frame.period)).normL2() < 0.1) {
            continue;
        }

        for (int j = 0; j < refineIters; ++j) {
            float period = frame.period * (lowRatio + j * spread);

            for (int k = 0; k < lookahead; ++k) {
                float delay = period * (float(k) * 1 + 1);
                int roundedDelay = roundToInt(delay);

                waveBuff = audio.sectionAtMost(offset, roundToInt(period));
                float power = waveBuff.normL2() + 0.0001f;

                Buffer<float> diff = diffBuff.withSize(waveBuff.size());

                if (offset - roundedDelay >= 0) {
                    offsetBuff = audio.sectionAtMost(offset - roundedDelay, waveBuff.size());
                    VecOps::sub(waveBuff, offsetBuff, diff);

                    subnorms[j] += diff.normL2() / power;
                }

                if (offset + roundedDelay + diff.size() <= numSamples) {
                    offsetBuff = audio.sectionAtMost(offset + roundedDelay, waveBuff.size());
                    VecOps::sub(waveBuff, offsetBuff, diff);

                    subnorms[j] += diff.normL2() / power;
                }
            }
        }

        // invert polarity to make quad interp find peak correctly
        subnorms.mul(-1.f);

        float maxVal;
        int maxIdx = refineIters / 2;
        subnorms.getMax(maxVal, maxIdx);
        float fIndex = interpIndexQuadratic(subnorms, maxIdx, 0);

        float interpPeriod = frame.period * (lowRatio + fIndex * spread);

        subnorms.mul(-1.f);

        bestNorm     = jmin(bestNorm, subnorms[maxIdx]);
        frame.atonal = 10.f * interpValueQuadratic(subnorms, maxIdx) / frame.period;
        frame.period = interpPeriod;
        cumeBest     += bestNorm;
    }

    return cumeBest / float(sample->periods.size());
}

float PitchTracker::interpIndexQuadratic(const Buffer<float>& norms, int troughIndex, int minlag) {
    if(troughIndex >= norms.size()) {
        return float (troughIndex + minlag);
    }

    float y1, y2, y3;

    // norms is already offset by minlag, so these statements are symmetical
    getAdjacentValues(norms, troughIndex, y1, y2, y3);
    return (float) troughIndex + minlag + Resampling::interpIndexQuadratic(y1, y2, y3);
}

float PitchTracker::interpValueQuadratic(const Buffer<float>& norms, int troughIndex) {
    if(troughIndex >= norms.size() || norms.empty()) {
        return 0;
    }

    float y1, y2, y3;
    getAdjacentValues(norms, troughIndex, y1, y2, y3);
    return Resampling::interpValueQuadratic(y1, y2, y3);
}

void PitchTracker::getAdjacentValues(Buffer<float> buff, int index, float& y1, float& y2, float& y3) {
    y2 = buff[index];
    y1 = index >= 1 ? buff[index - 1] : y2;
    y3 = index < buff.size() - 1 ? buff[index + 1] : y2;
}

int PitchTracker::getTrough(Buffer<float> norms, int minlag) {
    int troughIndex = -1;

    for (int i = 0; i < norms.size(); ++i) {
        if (norms[i] < aperiodicityThresh) {
            troughIndex = i;
            float minNorm = norms[i];

            int j = i + 1;
            while (j < norms.size() && norms[j] < aperiodicityThresh) {
                if (norms[j] < minNorm) {
                    troughIndex = j;
                    minNorm = norms[j];
                }

                ++j;
            }

            break;
        }
    }

    if(troughIndex < 0) {
        float minval;
        norms.getMin(minval, troughIndex);

        int realIndex = troughIndex + minlag;
        while (realIndex / 2 > minlag && norms[realIndex / 2 - minlag] / minval < data.octaveRatioThres) {
            realIndex /= 2;
        }

        troughIndex = realIndex - minlag;
        float minNorm = norms[troughIndex];

        for (int i = jmax(0, troughIndex - 5); i < jmin(10, norms.size() - troughIndex - 5); ++i) {
            if (minNorm < norms[i]) {
                troughIndex = i;
                minNorm = norms[i];
            }
        }
    }

    return troughIndex;
}

void PitchTracker::fillFrequencyBins() {
    bins.clear();

    for (auto& frame : sample->periods) {
        bool contained = false;

        for (auto& bin : bins) {
            if (bin.contains(frame.period)) {
                bin.averagePeriod = (bin.population * bin.averagePeriod + frame.period) / float(bin.population + 1);
                bin.population++;
                contained = true;
                break;
            }
        }

        if (!contained) {
            FrequencyBin bin{};
            bin.averagePeriod = frame.period;
            bin.population    = 1;
            bins.emplace_back(bin);
        }
    }

    if (bins.empty()) {
        return;
    }

    std::sort(bins.begin(), bins.end());

    float baseAverage = bins.back().averagePeriod;

    for (auto& frame : sample->periods) {
        float avgToCurrRatio = baseAverage / frame.period;
        auto intRatio = float(int(avgToCurrRatio + 0.12f));

        if (fabsf(intRatio - avgToCurrRatio) < 0.06f) {
            frame.period *= intRatio;
        } else {
            float currToAvgRatio = 1 / avgToCurrRatio;
            intRatio = float(int(currToAvgRatio + 0.12f));

            if(fabsf(float(intRatio) - currToAvgRatio) < 0.06f && intRatio > 0) {
                frame.period /= intRatio;
            } else {
                frame.period = baseAverage;
            }
        }
    }
}

vector<PitchTracker::FrequencyBin>& PitchTracker::getFrequencyBins() {
    return bins;
}

void PitchTracker::reset() {
    sample = nullptr;
    data.reset();
}

float PitchTracker::getAveragePeriod() {
    return bins.empty() ? 512 : bins.back().averagePeriod;
}

float PitchTracker::logTwo(float x) {
    static const float invLog2 = 1.f / logf(2.f);

    return logf(x) * invLog2;
}

float PitchTracker::erbsToHertz(float x) {
    return (powf(10.f, x / 21.4f) - 1.f) * 229;
}

float PitchTracker::hertzToErbs(float x) {
    return 21.4f * log10f(1 + x / 229);
}

void PitchTracker::createKernels(
        vector<Buffer<float>>& kernels,
        ScopedAlloc<float>& kernelMemory,
        const Buffer<int>& kernelSizes,
        Buffer<float> erbFreqs,
        Buffer<float> pitchCandidates) {
    int primes[] = {   1,    2,    3,    5,    7,   11,   13,   17,   19,   23,   29,   31,   37,   41,   43,   47,   53,
                      59,   61,   67,   71,   73,   79,   83,   89,   97,  101,  103,  107,  109,  113,  127,  131,  137,
                     139,  149,  151,  157,  163,  167,  173,  179,  181,  191,  193,  197,  199,  211,  223,  227,  229,
                     233,  239,  241,  251,  257,  263,  269,  271,  277,  281,  283,  293,  307,  311,  313,  317,  331,
                     337,  347,  349,  353,  359,  367,  373,  379,  383,  389,  397,  401,  409,  419,  421,  431,  433,
                     439,  443,  449,  457,  461,  463,  467,  479,  487,  491,  499,  503,  509,  521,  523,  541,  547,
                     557,  563,  569,  571,  577,  587,  593,  599,  601,  607,  613,  617,  619,  631,  641,  643,  647,
                     653,  659,  661,  673,  677,  683,  691,  701,  709,  719,  727,  733,  739,  743,  751,  757,  761,
                     769,  773,  787,  797,  809,  811,  821,  823,  827,  829,  839,  853,  857,  859,  863,  877,  881,
                     883,  887,  907,  911,  919,  929,  937,  941,  947,  953,  967,  971,  977,  983,  991,  997, 1009,
                    1013, 1019, 1021, 1031, 1033, 1039, 1049, 1051, 1061, 1063, 1069, 1087, 1091, 1093, 1097, 1103, 1109 };

    int numERBs       = erbFreqs.size();
    int numCandidates = kernelSizes.size();

    ScopedAlloc<float> erbScale(numERBs);
    erbFreqs.inv().sqrt();

    kernelMemory.resetPlacement();

    const float twoPi = MathConstants<float>::twoPi;

    for (int i = 0; i < numCandidates; ++i) {
        float candFreq = pitchCandidates[i];
        kernels[i] = kernelMemory.place(numERBs);
        int numHarmonics = roundToInt(erbFreqs.back() / candFreq - 0.75f);

        ScopedAlloc<float> buff(numERBs * 2);
        Buffer<float> q = buff.place(numERBs);
        Buffer<float> a = buff.place(numERBs);

        q.mul(erbFreqs, 1 / candFreq);
        kernels[i].zero();

        int primeIdx = 0;
        int startIdxA = 0, startIdxB = 0;

        while (primes[primeIdx] < numHarmonics) {
            int prime = primes[primeIdx];

            a.add(q, -prime).abs();

            for (int k = startIdxA; k < numERBs; ++k) {
                if (a[k] < 0.25f) {
                    startIdxA = k;

                    while (a[k] < 0.25f) {
                        kernels[i][k] = cosf(twoPi * a[k]);
                        ++k;
                    }
                    break;
                }
            }

            bool firstTrough = true;
            for (int k = startIdxB; k < numERBs; ++k) {
                if (a[k] >= 0.25f && a[k] < 0.75f) {
                    startIdxB = k;

                    while (a[k] >= 0.25f && a[k] < 0.75f) {
                        kernels[i][k] += cosf(twoPi * q[k]) / 2;
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

        kernels[i].mul(erbScale);
        kernels[i].mul(MathConstants<float>::sqrt2 / kernels[i].normL2());
    }
}

void PitchTracker::calcLambda(Window& window, const Buffer<float>& realErbIdx) {
    int start     = window.erbStart;
    int end       = window.erbEnd;
    int size      = end - start;
    int metaStart = 0;

    window.lambda.zero();
    Buffer<float> lambda = window.lambda.section(metaStart, size);

    window.lambda.add(realErbIdx.section(window.erbStart, size), window.erbOffset);

    ScopedAlloc<float> mu(size);

    mu.set(1.f);
    lambda.abs();
    mu.subCRev(1.f, lambda).copyTo(lambda);
}

void PitchTracker::swipe() {
    int pitchLimits[]     = { 28, 3000 };
    int hopCycles         = 4;
    float hannK           = 2.;
    float deltaPitchLog2  = 1 / 96.f;
    float deltaERBs       = 0.1f;                // ERB = equivalent rectangular bandwidth
    float strengthThresh  = 1.f;
    float lowLimitLog2    = logTwo((float) pitchLimits[0]);
    float highLimitLog2   = logTwo((float) pitchLimits[1]);
    int numCandidates     = int((highLimitLog2 - lowLimitLog2) / deltaPitchLog2 + 1);
    float samplerate      = sample->samplerate;
    float cumeTime        = 0;

    float sampleSeconds   = sample->audio.size() / samplerate;
    float deltaTime       = jmax(0.005f, 0.01f * sampleSeconds);
    int numTimes          = int(sampleSeconds / deltaTime) + 1;
    int logWinSizeHigh    = roundToInt(logTwo(4.f * hannK * samplerate / (float) pitchLimits[0]));
    int logWinSizeLow     = roundToInt(logTwo(4.f * hannK * samplerate / (float) pitchLimits[1]));
    int numWindows        = int(logWinSizeHigh - logWinSizeLow) + 1;

    ScopedAlloc<float> memory(numWindows * 2 + numCandidates * 7 + numTimes);
    Buffer<float> twos            = memory.place(numCandidates);
    Buffer<float> pitchCandLog2   = memory.place(numCandidates);
    Buffer<float> pitchCandidates = memory.place(numCandidates);
    Buffer<float> candLoudness    = memory.place(numCandidates);
    Buffer<float> realErbIdx      = memory.place(numCandidates);
    Buffer<float> relativeFreqs   = memory.place(numCandidates);
    Buffer<float> optimalFreqs    = memory.place(numWindows);
    Buffer<float> windowSizes     = memory.place(numWindows);
    Buffer<float> pitches         = memory.place(numTimes);

    /// calculate candidate frequencies
    twos.set(2.f);
    pitchCandLog2.ramp(lowLimitLog2, deltaPitchLog2);
    windowSizes.ramp((float) logWinSizeHigh, -1);

    pitchCandLog2.copyTo(pitchCandidates);
    pitchCandidates.powCRev(2.f);
    windowSizes.powCRev(2.f);
    VecOps::divCRev(windowSizes, 4 * hannK * samplerate, optimalFreqs);

    realErbIdx.set(logTwo(4 * hannK * samplerate / windowSizes.front()));
    realErbIdx.subCRev(1.f);
    realErbIdx.add(pitchCandLog2);

    float erbLow  = hertzToErbs(pitchCandidates.front() * 0.25f);
    float erbHigh = hertzToErbs(samplerate * 0.5f);
    int numERBs   = int((erbHigh - erbLow) / deltaERBs + 1);

    /// calculate ERBs
    ScopedAlloc<float> erbMem(numERBs * 2);
    Buffer<float> erbFreqs = erbMem.place(numERBs);
    Buffer<float> erbMagnitudes = erbMem.place(numERBs);

    erbFreqs.ramp(erbLow, deltaERBs);

    for(int i = 0; i < numERBs; ++i) {
        erbFreqs[i] = erbsToHertz(erbFreqs[i]);
    }

    /// calculate kernels
    ScopedAlloc<int>      kernelSizes(numCandidates);
    ScopedAlloc<float>    kernelMemory(numCandidates * numERBs);
    vector<Buffer<float>> kernels(numCandidates);

    createKernels(kernels, kernelMemory, kernelSizes, erbFreqs, pitchCandidates);

    // for(auto& k : kernels) {
    //     DEBUG_ADD_TO_HEATMAP("kernels", k);
    // }
    // DEBUG_HEATMAP("kernels");

    ScopedAlloc<float> winMemory(8192 * 3 + numCandidates);

    Buffer<float> signal = sample->audio.left;

    ScopedAlloc<float> strengthMatrix(numCandidates * numTimes);
    vector<StrengthColumn> strengthColumns;

    for(int i = 0; i < numTimes; ++i) {
        StrengthColumn sc;
        sc.column = strengthMatrix.section(i * numCandidates, numCandidates);
        sc.time   = i * deltaTime;

        strengthColumns.emplace_back(sc);
    }

    strengthMatrix.zero();

    /// initialize windows

    ScopedAlloc<float> spectFreqMem(roundToInt(2 * windowSizes.front()));
    ScopedAlloc<float> hannMemory(roundToInt(2 * windowSizes.front()));
    ScopedAlloc<float> lambdaMemory(numWindows * 2 * numCandidates);

    vector<Window> windows(numWindows);

    numWindows = 8;
    for (int i = 0; i < numWindows; ++i) {
        Window& window        = windows[i];
        window.index          = i;
        window.size           = (int) windowSizes[i];
        window.optimalFreq    = optimalFreqs[i];
        window.erbOffset      = -(1 + i);

        int hopSize           = roundToInt(hopCycles * samplerate / window.optimalFreq);
        window.overlapSamples = jmax(0, window.size - hopSize);
        window.offsetSamples  = 0;

        // solution to +-1 = (lowLimitLog2 + x * deltaPitchLog2 + 1 - logTwo(4 * hannK * samplerate / windowSizes.front()) + window.erbOffset;
        const float common = logTwo(hannK * samplerate / windowSizes.front()) - lowLimitLog2 - window.erbOffset;
        window.erbStart    = jlimit<int>(0, numCandidates, roundToInt(common / deltaPitchLog2));
        window.erbEnd      = jlimit<int>(0, numCandidates, roundToInt((2 + common) / deltaPitchLog2));
        window.erbSize     = window.erbEnd - window.erbStart;

        if(window.erbSize == 0) {
            continue;
        }

        window.hannWindow = hannMemory.place(window.size);
        window.hannWindow.hann();

        window.lambda = lambdaMemory.place(window.erbSize);
        calcLambda(window, realErbIdx);

        window.spectFreqs = spectFreqMem.place(window.size / 2);
        window.spectFreqs.ramp(0, samplerate / float(window.size));

        Transform fft;
        fft.setFFTScaleType(Transform::ScaleType::NoDivByAny);
        fft.allocate(window.size, true);

        cumeTime = 0;
        int totalSliceIndex = 0;

        ScopedAlloc<float> windowStrengths(numCandidates); // window.erbSize
        windowStrengths.zero();

        while (true) {
            int lastOffset     = window.offsetSamples;
            int signalPosStart = jmin(signal.size(), window.offsetSamples - window.size / 2);
            int signalPosEnd   = jmin(signal.size(), window.offsetSamples + window.size / 2);
            int paddingFront   = window.size / 2 - window.offsetSamples;
            int paddingBack    = window.size - jmin(window.size, signal.size() - window.offsetSamples);

            int timeSlicesThisWindow = 0;
            int startingSlice = totalSliceIndex;

            float prevTime = cumeTime;
            while((cumeTime) * samplerate < signalPosEnd && totalSliceIndex < numTimes - 1) {
                cumeTime += deltaTime;
                ++timeSlicesThisWindow;
                ++totalSliceIndex;
            }

            window.offsetSamples += window.overlapSamples;

            if(paddingBack >= window.size || (timeSlicesThisWindow == 0 && totalSliceIndex == numTimes)) {
                break;
            }

            if(timeSlicesThisWindow == 0) {
                continue;
            }

            winMemory.resetPlacement();
            Buffer<float> paddedSignal  = winMemory.place(window.size);
            Buffer<float> lastStrengths = winMemory.place(numCandidates);
            // Buffer<float> source;

            if (paddingFront > 0) {
                jassert(paddingFront <= window.size);

                paddedSignal.zero(paddingFront);

                int numToCopy = jmin(signal.size(), window.size - paddingFront);
                signal.copyTo(paddedSignal.section(paddingFront, numToCopy));

                if(signal.size() < window.size - paddingFront) {
                    paddedSignal.section(paddingFront + signal.size(), window.size - paddingFront - signal.size()).zero();
                }
            } else if (paddingBack > 0) {
                paddedSignal.zero();
                signal.section(signalPosStart, window.size - paddingBack).copyTo(paddedSignal);
            } else {
                signal.section(signalPosStart, window.size).copyTo(paddedSignal);
            }

            paddedSignal.mul(window.hannWindow);
            // DEBUG_ADD_TO_HEATMAP(String::formatted("source-%d", window.index), paddedSignal);

            lastStrengths.zero();

            fft.forward(paddedSignal);
            candLoudness.zero();

            Buffer<float> magnitudes = fft.getMagnitudes();

            // DEBUG_ADD_TO_HEATMAP(String::formatted("mag-%d", window.index), magnitudes.sectionAtMost(0, 512));

            int specIdx = 0;

            for(int k = 0; k < numERBs; ++k) {
                float candFreq = erbFreqs[k];

                while (specIdx < window.spectFreqs.size() && window.spectFreqs[specIdx] < candFreq) {
                    ++specIdx;
                }
                specIdx = jmin(specIdx, magnitudes.size() - 1);

                float interpMagn;

                // if (specIdx >= 3) {
                //     float* x = window.spectFreqs + (specIdx - 3);
                //     float* y = magnitudes + (specIdx - 3);
                //
                //     jassert(candFreq < x[3] && candFreq >= x[2]);
                //     interpMagn = Resampling::spline(x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3], candFreq);
                // } else
                if (specIdx >= 1 && candFreq < window.spectFreqs[specIdx] && candFreq >= window.spectFreqs[specIdx - 1]) {
                    float* x = window.spectFreqs + (specIdx - 1);
                    float* y = magnitudes + (specIdx - 1);
                    interpMagn = Resampling::lerp(x[0], y[0], x[1], y[1], candFreq);
                } else {
                    interpMagn = magnitudes[specIdx];
                }

                erbMagnitudes[k] = interpMagn;
            }

            erbMagnitudes.threshLT(0.f).sqrt();

            // DEBUG_ADD_TO_HEATMAP(String::formatted("erb-%d", window.index), erbMagnitudes);

            if(lastOffset > 0) {
                windowStrengths.copyTo(lastStrengths);
            }

            // for(int c = 0; c < window.erbSize; ++c) {
            for(int c = 0; c < numCandidates; ++c) {
                // windowStrengths[c] = kernels[c + window.erbStart].dot(erbMagnitudes);
                windowStrengths[c] = kernels[c].dot(erbMagnitudes);
            }

            if(lastOffset == 0) {
                windowStrengths.copyTo(lastStrengths);
            }

            // DEBUG_VIEW(windowStrengths, String::formatted("strengths-%d-%d", window.index, window.offsetSamples));
            // DEBUG_ADD_TO_HEATMAP(String::formatted("strengths-%d", window.index), windowStrengths);

            ScopedAlloc<float> weightedLoudness(numCandidates);

            float prevWindowTime = float(lastOffset) / samplerate;
            float thisWindowTime = float(window.offsetSamples) / samplerate;
            float diffTime = thisWindowTime - prevWindowTime;

            for (int s = 0; s < timeSlicesThisWindow; ++s) {
                StrengthColumn& sc = strengthColumns[startingSlice + s];

                float portion = diffTime == 0.f ? 1.f : (sc.time - prevWindowTime) / diffTime;

                weightedLoudness.zero();

                if(portion < 1.f) {
                    weightedLoudness.addProduct(lastStrengths, 1 - portion);
                }

                if(portion > 0.f) {
                    weightedLoudness.addProduct(windowStrengths, portion);
                }

                // if(window.erbStart > 0) {
                //     weightedLoudness.section(0, window.erbStart).zero();
                // }
                // weightedLoudness.section(window.erbStart, window.erbSize).mul(window.lambda);
                // if(window.erbEnd < numCandidates) {
                //     weightedLoudness.offset(window.erbEnd).zero();
                // }

                sc.column.section(0, numCandidates).add(weightedLoudness);
            }
        }
        // DEBUG_HEATMAP(String::formatted("erb-%d", window.index));
        // DEBUG_HEATMAP(String::formatted("mag-%d", window.index));
        // DEBUG_HEATMAP(String::formatted("source-%d", window.index));
        // DEBUG_HEATMAP(String::formatted("strengths-%d", window.index));
    }

    // for(auto& sc : strengthColumns) {
    //     DEBUG_ADD_TO_HEATMAP(String::formatted("strengths"), sc.column);
    // }
    // DEBUG_HEATMAP(String::formatted("strengths"));

    pitches.set(-1.f);
    float backupPitch = -1;

    for (int i = 0; i < numTimes; ++i) {
        int maxIndex;
        float maxValue;

        StrengthColumn& sc = strengthColumns[i];
        sc.column.getMax(maxValue, maxIndex);

        if (maxValue > strengthThresh) {
            // if(i > 10 && i < 14) {
            //     DEBUG_VIEW(sc.column, "strengths");
            // }

            pitches[i] = pitchCandidates[maxIndex];

            if (maxIndex > 0 && maxIndex < sc.column.size() - 1) {
                float y1 = sc.column[maxIndex - 1];
                float y2 = sc.column[maxIndex    ];
                float y3 = sc.column[maxIndex + 1];

                float d = (y1 - 2 * y2 + y3);
                if (d != 0.f) {
                    float p = 0.5 * (y1 - y3) / d;

                    y1 = pitchCandidates[maxIndex - 1];
                    y2 = pitchCandidates[maxIndex   ];
                    y3 = pitchCandidates[maxIndex + 1];

                    pitches[i] = y2 - 0.25f * (y1 - y3) * p;
                }
            }
        } else {
            backupPitch = pitchCandidates[maxIndex];
        }
    }

    // DEBUG_VIEW(pitches, String::formatted("pitches"));

    // DEBUG_CLEAR();

    vector<ContiguousRegion> regions;
    regions.emplace_back(0);

    for (int i = 1; i < numTimes; ++i) {
        float ratio = pitches[i] / pitches[i - 1];

        if (ratio > 1.4f || ratio < 0.7f) {
            regions.back().endIdx = i - 1;

            if (pitches[i] > 0.f) {
                regions.emplace_back(i);
            }
        } else {
            if (pitches[i] > 0.f) {
                ++regions.back().population;
            }
        }
    }

    regions.back().endIdx = numTimes - 1;
    std::sort(regions.begin(), regions.end());

    ContiguousRegion& bestRegion = regions.back();

    float startPitch = pitches[bestRegion.startIdx];

    if(startPitch < 0) {
        startPitch = backupPitch;
    }

    int starts[] = {0, bestRegion.endIdx};
    int ends[] = {bestRegion.startIdx, numTimes - 1};

    for (int j = 0; j < 2; ++j) {
        for (int i = starts[j]; i <= ends[j]; ++i) {
            float pitch = pitches[i];

            if (pitch > 0) {
                float avgToCurrRatio = pitch / startPitch;
                auto intRatio = float(int(avgToCurrRatio + 0.12f));

                if (fabsf(intRatio - avgToCurrRatio) < 0.06f && intRatio > 0) {
                    pitch /= intRatio;
                } else {
                    float currToAvgRatio = 1 / avgToCurrRatio;
                    intRatio = float(int(currToAvgRatio + 0.12f));

                    if (fabsf(float(intRatio) - currToAvgRatio) < 0.06f && intRatio > 0) {
                        pitch *= intRatio;
                    } else {
                        pitch = startPitch;
                    }
                }
            } else {
                pitch = startPitch;
            }

            pitches[i] = pitch;
        }
    }

    sample->resetPeriods();

    float prevPeriod = 337;
    for (int i = 0; i < numTimes; ++i) {
        PitchFrame frame;
        frame.period = pitches[i] < 0 ? prevPeriod : samplerate / pitches[i];
        frame.sampleOffset = roundToInt(jmax(0.f, strengthColumns[i].time) * samplerate);

        if(frame.sampleOffset == 0) {
            continue;
        }

        prevPeriod = frame.period;
        sample->addFrame(frame);

        if (i < numTimes - 1) {
            frame.sampleOffset += roundToInt(jmax(0.f, strengthColumns[i + 1].time) * samplerate);
            frame.sampleOffset /= 2;

            sample->addFrame(frame);
        }
    }

    float averagePitch = 0;

    int numGood = 0;
    for (int i = bestRegion.startIdx; i <= bestRegion.endIdx; ++i) {
        if (pitches[i] > 0) {
            averagePitch += pitches[i];
            ++numGood;
        }
    }

    if(numGood == 0) {
        averagePitch = backupPitch;
    } else {
        averagePitch /= float(numGood);
    }

    float averagePeriod = samplerate / averagePitch;

    FrequencyBin bin{};
    bin.averagePeriod = averagePeriod;

    bins.clear();
    bins.emplace_back(bin);

    confidence = refineFrames(sample, averagePeriod);
    DEBUG_PERIODS(*sample, "after-refine-swipe");
}

void PitchTracker::trackPitch() {
    if (algo == AlgoSwipe || sample->size() < 8192) {
        swipe();
    } else if (algo == AlgoYin) {
        yin();
    } else {
        yin();

        float yinAtonicity = 0;
        vector<PitchFrame> yinFrames = sample->periods;

        for(auto& yinFrame : yinFrames) {
            yinAtonicity += yinFrame.atonal;
        }

        yinAtonicity /= float(yinFrames.size());

        if (yinAtonicity > 2.f) {
            swipe();
            vector<PitchFrame> swipeFrames = sample->periods;

            float swipeAtonicity = 0;

            for(auto& swipeFrame : swipeFrames) {
                swipeAtonicity += swipeFrame.atonal;
            }

            swipeAtonicity /= float(swipeFrames.size());

            if(yinAtonicity < swipeAtonicity) {
                sample->periods = yinFrames;
            }
        }
    }
}

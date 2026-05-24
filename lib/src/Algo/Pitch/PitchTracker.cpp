#include <Util/Arithmetic.h>
#include <Array/VecOps.h>
#include "PitchTracker.h"
#include "PeriodSpectralPitchScorer.h"
#include "PitchKernelBuilder.h"
#include "SwipePitchDetector.h"
#include "YinPitchDetector.h"
#include "../FFT.h"
#include "../Resampling.h"
#include "../../Audio/PitchedSample.h"
#include "../../Util/NumberUtils.h"
#include "../../../tests/TestDefs.h"

#ifdef BUILD_TESTING
  #define DEBUG_RENDERING
#endif

#include "../../../tests/SignalDebugger.h"

namespace {

constexpr float maxYinAtonicityForAuto = 10.f;
constexpr float maxYinAtonicityForSubharmonicCheck = 40.f;
constexpr float minSpectralSubharmonicImprovement = 0.65f;

float averageAtonal(const vector<PitchFrame>& frames) {
    if (frames.empty()) {
        return 0.f;
    }

    float sum = 0.f;
    for (const auto& frame: frames) {
        sum += frame.atonal;
    }

    return sum / float(frames.size());
}

float averagePeriod(const vector<PitchFrame>& frames, int fallbackNote, int sampleRate) {
    if (frames.empty()) {
        return fallbackNote > 10
            ? sampleRate / NumberUtils::noteToFrequency(fallbackNote)
            : 0.f;
    }

    float sum = 0.f;
    for (const auto& frame: frames) {
        sum += frame.period;
    }

    return sum / float(frames.size());
}

void retuneFramesToMidiNote(PitchedSample* sample, float currentAveragePeriod, int midiNote) {
    if (sample == nullptr || currentAveragePeriod <= 0.f) {
        return;
    }

    const float targetPeriod = sample->samplerate / (float) NumberUtils::noteToFrequency(midiNote);
    const float scale = targetPeriod / currentAveragePeriod;

    for (auto& frame: sample->periods) {
        if (frame.period < currentAveragePeriod * 0.5f || frame.period > currentAveragePeriod * 1.5f) {
            frame.period = targetPeriod;
        } else {
            frame.period *= scale;
        }
    }
}

void logPitchDecision(PitchedSample* sample,
                      const String& decision,
                      float confidence,
                      float yinAtonicity = -1.f,
                      float swipeAtonicity = -1.f) {
    if (sample == nullptr) {
        DBG("PitchTracker decision=" + decision + " sample=null");
        return;
    }

    float period = averagePeriod(sample->periods, sample->fundNote, sample->samplerate);
    float frequency = period > 0.f ? sample->samplerate / period : 0.f;
    int detectedNote = frequency > 0.f ? roundToInt(NumberUtils::frequencyToNote(frequency)) : -1;

    float minPeriod = 1.e30f;
    float maxPeriod = 0.f;
    float minOffset = 1.e30f;
    float maxOffset = 0.f;

    for (const auto& frame: sample->periods) {
        minPeriod = jmin(minPeriod, frame.period);
        maxPeriod = jmax(maxPeriod, frame.period);
        minOffset = jmin(minOffset, (float) frame.sampleOffset);
        maxOffset = jmax(maxOffset, (float) frame.sampleOffset);
    }

    if (sample->periods.empty()) {
        minPeriod = 0.f;
        minOffset = 0.f;
    }

    String name = sample->lastLoadedFilePath.isNotEmpty()
        ? sample->lastLoadedFilePath
        : sample->uniqueName;

    DBG("PitchTracker decision"
        + String(" algo=") + decision
        + " sample=\"" + name + "\""
        + " frames=" + String((int) sample->periods.size())
        + " sr=" + String(sample->samplerate)
        + " samples=" + String(sample->size())
        + " fileFundNote=" + String(sample->fundNote)
        + " detectedNote=" + String(detectedNote)
        + " freqHz=" + String(frequency, 3)
        + " avgPeriod=" + String(period, 3)
        + " minPeriod=" + String(minPeriod, 3)
        + " maxPeriod=" + String(maxPeriod, 3)
        + " firstOffset=" + String(minOffset, 1)
        + " lastOffset=" + String(maxOffset, 1)
        + " confidence=" + String(confidence, 3)
        + " yinAtonicity=" + String(yinAtonicity, 3)
        + " swipeAtonicity=" + String(swipeAtonicity, 3));
}

}

PitchTracker::PitchTracker() :
        algo(AlgoAuto)
    ,   aperiodicityThresh(0.3f) {
    reset();
}

void PitchTracker::yin() {
    YinPitchDetector::track(*this);
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
            } else if (frame.period < baseAverage) {
                frame.period = baseAverage;
            }
        }
    }

    int onsetEnd = 0;
    while (onsetEnd < sample->periods.size()
           && sample->periods[onsetEnd].period <= baseAverage * 1.02f
           && sample->periods[onsetEnd].atonal > 0.6f) {
        ++onsetEnd;
    }

    if (onsetEnd > 0
        && onsetEnd < sample->periods.size()
        && sample->periods[onsetEnd].period > baseAverage * 1.05f) {
        const float periodDelta = sample->periods[onsetEnd].period - baseAverage;

        for (int i = 0; i < onsetEnd; ++i) {
            sample->periods[i].period = baseAverage + periodDelta * float(onsetEnd - i + 1);
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
    int numERBs       = erbFreqs.size();
    int numCandidates = kernelSizes.size();

    ScopedAlloc<float> erbScale(numERBs);
    erbFreqs.inv().sqrt();

    kernelMemory.resetPlacement();

    for (int i = 0; i < numCandidates; ++i) {
        float candFreq = pitchCandidates[i];
        kernels[i] = kernelMemory.place(numERBs);
        int numHarmonics = roundToInt(erbFreqs.back() / candFreq - 0.75f);

        ScopedAlloc<float> buff(numERBs * 2);
        Buffer<float> q = buff.place(numERBs);
        Buffer<float> a = buff.place(numERBs);

        VecOps::mul(erbFreqs, 1 / candFreq, q);
        kernels[i].zero();

        PitchKernelBuilder::paintPrimeHarmonics(kernels[i], q, a, numHarmonics);

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

    window.lambda
        .add(realErbIdx.section(window.erbStart, size))
        .add(window.erbOffset);

    ScopedAlloc<float> mu(size);

    mu.set(1.f);
    lambda.abs();
    VecOps::subCRev(lambda, 1.f, mu);
    mu.copyTo(lambda);
}

void PitchTracker::swipe() {
    SwipePitchDetector::track(*this);
}

void PitchTracker::trackPitch(bool logDecision) {
    if (algo == AlgoSwipe || sample->size() < 8192) {
        swipe();
        if (logDecision) {
            logPitchDecision(sample, "swipe", confidence);
        }
    } else if (algo == AlgoYin) {
        yin();
        if (logDecision) {
            logPitchDecision(sample, "yin", confidence);
        }
    } else {
        yin();

        float yinAtonicity = 0;
        vector<PitchFrame> yinFrames = sample->periods;

        yinAtonicity = averageAtonal(yinFrames);
        String decision = "auto-yin";
        float swipeAtonicity = -1.f;

        if (yinAtonicity > maxYinAtonicityForAuto) {
            swipe();
            vector<PitchFrame> swipeFrames = sample->periods;

            swipeAtonicity = averageAtonal(swipeFrames);
            decision = "auto-swipe";

            if(yinAtonicity < swipeAtonicity) {
                sample->periods = yinFrames;
                decision = "auto-yin-retained";
            } else if (yinAtonicity < maxYinAtonicityForSubharmonicCheck) {
                const float swipeAveragePeriod = averagePeriod(swipeFrames, sample->fundNote, sample->samplerate);
                const int detectedMidiNote = roundToInt(
                    NumberUtils::frequencyToNote(sample->samplerate / swipeAveragePeriod));

                float currentSpectralScore = 0.f;
                float bestSpectralScore = 0.f;
                PeriodSpectralPitchScorer::Context spectralContext;
                const int subharmonicMidiNote = PeriodSpectralPitchScorer::findBestSubharmonicMidiNote(
                    *sample, detectedMidiNote, spectralContext, currentSpectralScore, bestSpectralScore);

                if (subharmonicMidiNote < detectedMidiNote - 12
                    && bestSpectralScore < currentSpectralScore * minSpectralSubharmonicImprovement) {
                    retuneFramesToMidiNote(sample, swipeAveragePeriod, subharmonicMidiNote);
                    decision = "auto-swipe-subharmonic";
                }
            }
        }

        if (logDecision) {
            logPitchDecision(sample, decision, confidence, yinAtonicity, swipeAtonicity);
        }
    }
}

#include "SwipePitchDetector.h"

#include <Array/VecOps.h>

#include "../FFT.h"
#include "../Resampling.h"
#include "PitchTracker.h"
#include "../../Audio/PitchedSample.h"
#include "../../../tests/TestDefs.h"

void SwipePitchDetector::track(PitchTracker& tracker) {
    using StrengthColumn = PitchTracker::StrengthColumn;
    using ContiguousRegion = PitchTracker::ContiguousRegion;
    using Window = PitchTracker::Window;

    PitchedSample* sample = tracker.sample;
    if (sample == nullptr) {
        return;
    }

    int hopCycles         = 4;
    float hannK           = 2.;
    float deltaPitchLog2  = 1 / 96.f;
    float deltaERBs       = 0.1f;
    float strengthThresh  = 1.f;
    float lowLimitLog2    = PitchTracker::logTwo(tracker.request.minFrequencyHz);
    float highLimitLog2   = PitchTracker::logTwo(tracker.request.maxFrequencyHz);
    int numCandidates     = int((highLimitLog2 - lowLimitLog2) / deltaPitchLog2 + 1);
    float samplerate      = sample->samplerate;
    float cumeTime        = 0;

    float sampleSeconds   = sample->audio.size() / samplerate;
    float deltaTime       = jmax(0.005f, 0.01f * sampleSeconds);
    int numTimes          = int(sampleSeconds / deltaTime) + 1;
    int logWinSizeHigh    = roundToInt(PitchTracker::logTwo(4.f * hannK * samplerate / tracker.request.minFrequencyHz));
    int logWinSizeLow     = roundToInt(PitchTracker::logTwo(4.f * hannK * samplerate / tracker.request.maxFrequencyHz));
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

    twos.set(2.f);
    pitchCandLog2.ramp(lowLimitLog2, deltaPitchLog2);
    windowSizes.ramp((float) logWinSizeHigh, -1);

    pitchCandLog2.copyTo(pitchCandidates);
    pitchCandidates.powCRev(2.f);
    windowSizes.powCRev(2.f);
    VecOps::divCRev(windowSizes, 4 * hannK * samplerate, optimalFreqs);

    realErbIdx.set(PitchTracker::logTwo(4 * hannK * samplerate / windowSizes.front()));
    realErbIdx.subCRev(1.f);
    realErbIdx.add(pitchCandLog2);

    float erbLow  = PitchTracker::hertzToErbs(pitchCandidates.front() * 0.25f);
    float erbHigh = PitchTracker::hertzToErbs(samplerate * 0.5f);
    int numERBs   = int((erbHigh - erbLow) / deltaERBs + 1);

    ScopedAlloc<float> erbMem(numERBs * 2);
    Buffer<float> erbFreqs = erbMem.place(numERBs);
    Buffer<float> erbMagnitudes = erbMem.place(numERBs);

    erbFreqs.ramp(erbLow, deltaERBs);

    for(int i = 0; i < numERBs; ++i) {
        erbFreqs[i] = PitchTracker::erbsToHertz(erbFreqs[i]);
    }

    ScopedAlloc<int>      kernelSizes(numCandidates);
    ScopedAlloc<float>    kernelMemory(numCandidates * numERBs);
    vector<Buffer<float>> kernels(numCandidates);

    PitchTracker::createKernels(kernels, kernelMemory, kernelSizes, erbFreqs, pitchCandidates);

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

        const float common = PitchTracker::logTwo(hannK * samplerate / windowSizes.front()) - lowLimitLog2 - window.erbOffset;
        window.erbStart    = jlimit<int>(0, numCandidates, roundToInt(common / deltaPitchLog2));
        window.erbEnd      = jlimit<int>(0, numCandidates, roundToInt((2 + common) / deltaPitchLog2));
        window.erbSize     = window.erbEnd - window.erbStart;

        if(window.erbSize == 0) {
            continue;
        }

        window.hannWindow = hannMemory.place(window.size);
        window.hannWindow.hann();

        window.lambda = lambdaMemory.place(window.erbSize);
        PitchTracker::calcLambda(window, realErbIdx);

        window.spectFreqs = spectFreqMem.place(window.size / 2);
        window.spectFreqs.ramp(0, samplerate / float(window.size));

        Transform fft;
        fft.allocate(window.size, Transform::ScaleType::NoDivByAny, true);

        cumeTime = 0;
        int totalSliceIndex = 0;

        ScopedAlloc<float> windowStrengths(numCandidates);
        windowStrengths.zero();

        while (true) {
            int lastOffset     = window.offsetSamples;
            int signalPosStart = jmin(signal.size(), window.offsetSamples - window.size / 2);
            int signalPosEnd   = jmin(signal.size(), window.offsetSamples + window.size / 2);
            int paddingFront   = window.size / 2 - window.offsetSamples;
            int paddingBack    = window.size - jmin(window.size, signal.size() - window.offsetSamples);

            int timeSlicesThisWindow = 0;
            int startingSlice = totalSliceIndex;

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
            lastStrengths.zero();

            fft.forward(paddedSignal);
            candLoudness.zero();

            Buffer<float> magnitudes = fft.getMagnitudes();

            int specIdx = 0;

            for(int k = 0; k < numERBs; ++k) {
                float candFreq = erbFreqs[k];

                while (specIdx < window.spectFreqs.size() && window.spectFreqs[specIdx] < candFreq) {
                    ++specIdx;
                }
                specIdx = jmin(specIdx, magnitudes.size() - 1);

                float interpMagn;

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

            if(lastOffset > 0) {
                windowStrengths.copyTo(lastStrengths);
            }

            for(int c = 0; c < numCandidates; ++c) {
                windowStrengths[c] = kernels[c].dot(erbMagnitudes);
            }

            if(lastOffset == 0) {
                windowStrengths.copyTo(lastStrengths);
            }

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

                sc.column.section(0, numCandidates).add(weightedLoudness);
            }
        }
    }

    pitches.set(-1.f);
    float backupPitch = -1;

    for (int i = 0; i < numTimes; ++i) {
        int maxIndex;
        float maxValue;

        StrengthColumn& sc = strengthColumns[i];
        sc.column.getMax(maxValue, maxIndex);

        if (maxValue > strengthThresh) {
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

    PitchTracker::FrequencyBin bin{};
    bin.averagePeriod = averagePeriod;

    tracker.bins.clear();
    tracker.bins.emplace_back(bin);

    tracker.confidence = PitchTracker::refineFrames(sample, averagePeriod);
    DEBUG_PERIODS(*sample, "after-refine-swipe");
}

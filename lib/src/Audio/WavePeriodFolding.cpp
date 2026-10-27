#include <Algo/Resampling.h>
#include <Array/ScopedAlloc.h>
#include <Array/VecOps.h>
#include <Audio/PitchedSample.h>

#include "WavePeriodFolding.h"

void WavePeriodFolding::render(
        const PitchedSample& sample,
        std::vector<Column>& columns,
        int longestSampleFrames,
        bool wrapCycles,
        bool interpolateCycles,
        double stereoPan,
        bool logMapping,
        bool useColumnPositions) {
    if (columns.size() < 2 || sample.periods.size() < 3
            || sample.size() < 2 || longestSampleFrames < 2) {
        for (auto& column : columns) {
            column.zero();
        }
        return;
    }

    const int rowCount = columns.front().size();
    const int quarter = rowCount / 4;
    const int sampleLength = sample.size();
    if (rowCount < 4 || quarter == 0) {
        return;
    }

    ScopedAlloc<float> fadeMemory(rowCount);
    Buffer<float> fadeInUp = fadeMemory.place(quarter);
    Buffer<float> fadeInDown = fadeMemory.place(quarter);
    Buffer<float> fadeOutUp = fadeMemory.place(quarter);
    Buffer<float> fadeOutDown = fadeMemory.place(quarter);
    fadeOutUp.ramp().sqr();
    VecOps::subCRev(fadeOutUp, 1.f, fadeOutDown);
    VecOps::flip(fadeOutDown, fadeInUp);
    VecOps::subCRev(fadeInUp, 1.f, fadeInDown);

    int paddedLength = rowCount;
    if (useColumnPositions) {
        for (const auto& frame : sample.periods) {
            paddedLength = juce::jmax(paddedLength, juce::roundToInt(frame.period * 1.25f));
        }
    }
    ScopedAlloc<float> workMemory(rowCount * 3 + paddedLength + 2 * quarter + sampleLength);
    Buffer<float> paddedCycle = workMemory.place(paddedLength);
    Buffer<float> resampledNext = workMemory.place(rowCount + quarter);
    Buffer<float> resampledPrevious = workMemory.place(rowCount + quarter);
    Buffer<float> combinedAudio = workMemory.place(sampleLength);
    Buffer<float> firstCycle = workMemory.place(rowCount);

    const float leftGain = sample.audio.numChannels == 1 ? 1.f : (float) (1.0 - stereoPan);
    VecOps::mul(sample.audio.left, leftGain, combinedAudio);
    if (sample.audio.numChannels > 1) {
        combinedAudio.addProduct(sample.audio.right, (float) stereoPan);
    }
    combinedAudio.mul(0.5f);

    const auto wrapCycle = [quarter, rowCount, fadeInUp, fadeInDown](Buffer<float> cycle) {
        Buffer<float> head = cycle.withSize(quarter);
        Buffer<float> tail = (cycle + rowCount).withSize(quarter);
        head.mul(fadeInUp);
        head.addProduct(tail, fadeInDown);
    };

    const auto resampleCycle = [&](int offset, int length, Buffer<float> destination) {
        Buffer<float> source = combinedAudio.sectionAtMost(offset, length);
        if (source.empty()) {
            destination.zero();
            return;
        }
        if (source.size() < length) {
            paddedCycle.zero();
            source.copyTo(paddedCycle);
            source = useColumnPositions ? paddedCycle.withSize(length) : paddedCycle;
        }
        Resampling::linResample(source, destination);
    };

    const float xIncrement = 1.f / (float) (columns.size() - 1);
    const float inverseLength = 1.f / (float) longestSampleFrames;
    const int offsetSamples = (int) (sample.phaseOffset * sample.getAveragePeriod());
    int nextCycle = 1;
    int oldCycle = 0;
    int previousOffset = 0;
    int nextOffset = 0;
    int previousLength = 0;
    int nextLength = 0;
    float previousX = 0.f;
    juce::String mappingLog;
    if (logMapping) {
        mappingLog = "VisualDsp wave-period-map"
                + juce::String(" columns=") + juce::String((int) columns.size())
                + " periods=" + juce::String((int) sample.periods.size())
                + " wavLength=" + juce::String(sampleLength)
                + " longestSample=" + juce::String(longestSampleFrames)
                + " fundNote=" + juce::String(sample.fundNote)
                + " avgPeriod=" + juce::String(sample.getAveragePeriod(), 3)
                + " samples=";
    }

    for (int index = 0; index < (int) columns.size(); ++index) {
        const float position = useColumnPositions ? columns[(size_t) index].x
                : (float) index * xIncrement;
        while (nextCycle < (int) sample.periods.size() - 1
                && (sample.periods[(size_t) nextCycle].sampleOffset - offsetSamples)
                        * inverseLength < position) {
            ++nextCycle;
        }

        const PitchFrame& nextFrame = sample.periods[(size_t) nextCycle];
        Buffer<float> column = columns[(size_t) index];
        if (nextFrame.period < 10.f) {
            column.zero();
            continue;
        }

        if (oldCycle != nextCycle) {
            const int previousCycle = juce::jmax(0, nextCycle - 1);
            const PitchFrame& previousFrame = sample.periods[(size_t) previousCycle];
            previousLength = juce::roundToInt(previousFrame.period * 1.25f);
            nextLength = juce::roundToInt(nextFrame.period * 1.25f);
            previousOffset = juce::jmax(0, previousFrame.sampleOffset - offsetSamples);
            nextOffset = juce::jmax(0, nextFrame.sampleOffset - offsetSamples);
            previousX = juce::jmin(1.f, previousOffset * inverseLength);

            if (oldCycle == 0 || (useColumnPositions && oldCycle != nextCycle - 1)) {
                if (nextOffset + nextLength < sampleLength || useColumnPositions) {
                    resampleCycle(previousOffset, previousLength, resampledPrevious);
                    if (oldCycle == 0) {
                        resampledPrevious.copyTo(firstCycle);
                    }
                    if (wrapCycles) {
                        wrapCycle(resampledPrevious);
                        if (oldCycle != 0) {
                            wrapCycle(resampledPrevious);
                        }
                    }
                } else {
                    resampledPrevious.zero(rowCount);
                    resampledNext.zero(rowCount);
                }
            } else {
                resampledNext.copyTo(resampledPrevious.withSize(rowCount));
            }

            if (nextOffset < sampleLength) {
                resampleCycle(nextOffset, nextLength, resampledNext);
                if (wrapCycles) {
                    wrapCycle(resampledNext);
                    wrapCycle(resampledNext);
                }
            }
        }

        oldCycle = nextCycle;
        const float distance = position - previousX;
        const float span = juce::jmin(1.f, nextOffset * inverseLength) - previousX;
        const float portion = juce::jmin(1.f, distance / span);
        if (logMapping
                && (index == 0 || index == (int) columns.size() / 8
                        || index == (int) columns.size() / 4
                        || index == (int) columns.size() / 2
                        || index == 3 * (int) columns.size() / 4
                        || index == 7 * (int) columns.size() / 8
                        || index == (int) columns.size() - 1)) {
            mappingLog += juce::String::formatted(
                    "[col=%d x=%.4f cycle=%d prev=%d next=%d period=%.3f portion=%.3f] ",
                    index, position, nextCycle, previousOffset, nextOffset,
                    nextFrame.period, portion);
        }

        if (!useColumnPositions && nextOffset + nextLength >= sampleLength) {
            column.zero();
        } else if (index == 0) {
            firstCycle.copyTo(column);
        } else if (interpolateCycles) {
            if (portion == 0.f) {
                resampledPrevious.copyTo(column);
            } else {
                VecOps::mul(resampledPrevious.withSize(rowCount), 1.f - portion, column);
                column.addProduct(resampledNext, portion);
            }
        } else if (portion < 0.9f) {
            resampledPrevious.copyTo(column);
        } else {
            resampledNext.copyTo(column);
        }
    }

    if (logMapping) {
        DBG(mappingLog);
    }
}

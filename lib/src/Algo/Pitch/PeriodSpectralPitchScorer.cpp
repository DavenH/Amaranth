#include "PeriodSpectralPitchScorer.h"

#include <Array/VecOps.h>

#include "../Resampling.h"
#include "../../Util/NumberUtils.h"

PeriodSpectralPitchScorer::Context::Context(int fftSize) :
        fftSize     (fftSize)
    ,   memory      (fftSize * 4)
    ,   cycle       (memory.place(fftSize))
    ,   absCycle    (memory.place(fftSize))
    ,   previous    (memory.place(fftSize / 2))
    ,   current     (memory.place(fftSize / 2))
    ,   diff        (memory.place(fftSize / 2)) {
    transform.allocate(fftSize, Transform::ScaleType::NoDivByAny, true);
}

float PeriodSpectralPitchScorer::scoreMidiNote(PitchedSample& sample, int midiNote, Context& context) {
    const float frequency = (float) NumberUtils::noteToFrequency(midiNote);
    const int period = roundToInt((float) sample.samplerate / frequency);
    const int numCycles = sample.size() / period;

    if (period < 8 || numCycles < 3) {
        return std::numeric_limits<float>::infinity();
    }

    double sum = 0.;
    int comparisons = 0;

    for (int cycleIndex = 0; cycleIndex < numCycles; ++cycleIndex) {
        const int offset = cycleIndex * period;
        Buffer<float> source = sample.audio.left.section(offset, period);
        Resampling::linResample(source, context.cycle);

        context.cycle.copyTo(context.absCycle);
        context.absCycle.abs();
        context.cycle.mul(1.f / jmax(0.01f, context.absCycle.max()));

        context.transform.forward(context.cycle);
        context.transform.getMagnitudes().copyTo(context.current);

        const float norm = context.current.normL2();
        context.current.mul(norm > 1.e-6f ? 1.f / norm : 0.f);

        if (cycleIndex > 0) {
            VecOps::sub(context.current, context.previous, context.diff);
            sum += context.diff.normL2();
            ++comparisons;
        }

        context.current.copyTo(context.previous);
    }

    return comparisons > 0
        ? (float) (sum / (double) comparisons)
        : std::numeric_limits<float>::infinity();
}

int PeriodSpectralPitchScorer::findBestSubharmonicMidiNote(
        PitchedSample& sample,
        int detectedMidiNote,
        Context& context,
        float& currentScore,
        float& bestScore) {
    currentScore = scoreMidiNote(sample, detectedMidiNote, context);
    bestScore = currentScore;
    int bestMidiNote = detectedMidiNote;

    for (int divisor = 3; divisor <= 6; ++divisor) {
        const int centerMidiNote = roundToInt((float) detectedMidiNote - 12.f * log2f((float) divisor));

        for (int midiNote = centerMidiNote - 2; midiNote <= centerMidiNote + 2; ++midiNote) {
            const float score = scoreMidiNote(sample, midiNote, context);

            if (score < bestScore) {
                bestScore = score;
                bestMidiNote = midiNote;
            }
        }
    }

    return bestMidiNote;
}

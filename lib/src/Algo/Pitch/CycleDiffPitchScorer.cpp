#include "CycleDiffPitchScorer.h"

#include <Array/VecOps.h>

void CycleDiffPitchScorer::precomputePeriods(
        const PitchTrackingRequest& request,
        double frequencyOfA4,
        int sampleRate,
        vector<int>& periods) {
    periods.resize(request.numMidiNotes);

    for (int i = 0; i < request.numMidiNotes; ++i) {
        const float freq = MidiMessage::getMidiNoteInHertz(i + request.firstMidiNote, frequencyOfA4);
        periods[i] = jmax(2, roundToInt((float) sampleRate / freq));
    }
}

float CycleDiffPitchScorer::scorePeriod(Buffer<float> signal, int period, Buffer<float> scratch) {
    const int numCycles = signal.size() / period;

    if (numCycles < 2) {
        return std::numeric_limits<float>::infinity();
    }

    const int numComparisons = numCycles - 1;
    const int diffLen        = numComparisons * period;

    Buffer<float> base   (signal.get(),          diffLen);
    Buffer<float> lagged (signal.get() + period, diffLen);
    Buffer<float> diff   (scratch.get(),         diffLen);
    VecOps::sub(lagged, base, diff);

    return diff.normL2() / (sqrtf((float) period) * (float) numComparisons);
}

void CycleDiffPitchScorer::scorePeriods(
        Buffer<float> signal,
        const vector<int>& periods,
        Buffer<float> scores,
        Buffer<float> scratch) {
    for (int i = 0; i < jmin((int) periods.size(), scores.size()); ++i) {
        scores[i] = scorePeriod(signal, periods[i], scratch);
    }
}

void CycleDiffPitchScorer::preferHigherOctaves(Buffer<float> scores, float octaveTolerance) {
    for (int key = 0; key + 12 < scores.size(); ++key) {
        if (scores[key] == std::numeric_limits<float>::infinity()) {
            continue;
        }
        if (scores[key + 12] == std::numeric_limits<float>::infinity()) {
            continue;
        }

        if (scores[key + 12] <= scores[key] * octaveTolerance) {
            scores[key] *= 2.f;
        }
    }
}

int CycleDiffPitchScorer::findBestScore(Buffer<float> scores, int initialIndex, float& bestScore) {
    int bestIndex = jlimit(0, scores.size() - 1, initialIndex);
    bestScore = std::numeric_limits<float>::infinity();

    for (int i = 0; i < scores.size(); ++i) {
        if (scores[i] < bestScore) {
            bestScore = scores[i];
            bestIndex = i;
        }
    }

    return bestIndex;
}

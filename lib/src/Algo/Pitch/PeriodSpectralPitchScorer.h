#pragma once

#include <limits>

#include "../FFT.h"
#include "../../Audio/PitchedSample.h"
#include "../../Array/ScopedAlloc.h"

class PeriodSpectralPitchScorer {
public:
    struct Context {
        explicit Context(int fftSize = 2048);

        int fftSize;
        Transform transform;
        ScopedAlloc<float> memory;
        Buffer<float> cycle;
        Buffer<float> absCycle;
        Buffer<float> previous;
        Buffer<float> current;
        Buffer<float> diff;
    };

    static float scoreMidiNote(PitchedSample& sample, int midiNote, Context& context);

    static int findBestSubharmonicMidiNote(
        PitchedSample& sample,
        int detectedMidiNote,
        Context& context,
        float& currentScore,
        float& bestScore);
};

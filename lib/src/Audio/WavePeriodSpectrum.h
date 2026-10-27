#pragma once

#include <vector>

#include "../Array/Column.h"

class Transform;

// Cycle-synchronous analysis. Output row zero is harmonic one, never DC.
class WavePeriodSpectrum {
public:
    static int harmonicCount(int legacyMidiNote, double sampleRate = 44100.0);
    static int cycleSize(int legacyMidiNote, double sampleRate = 44100.0);
    static void render(
            Transform& transform,
            const std::vector<Column>& cycles,
            std::vector<Column>& magnitudes,
            std::vector<Column>* phases = nullptr);
};

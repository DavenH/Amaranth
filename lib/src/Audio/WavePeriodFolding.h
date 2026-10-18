#pragma once

#include <vector>

#include "../Array/Column.h"

class PitchedSample;

class WavePeriodFolding {
public:
    // Columns own their storage. The pitch frames and audio remain owned by sample.
    static void render(
            const PitchedSample& sample,
            std::vector<Column>& columns,
            int longestSampleFrames,
            bool wrapCycles,
            bool interpolateCycles,
            double stereoPan,
            bool logMapping = false);
};

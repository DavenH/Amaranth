#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace CycleV2 {

// A fixed voice-time axis with an auditionable note-off and room for the longest tail.
class PreviewTraversal {
public:
    explicit PreviewTraversal(size_t columns, double releaseDuration = 0., double noteOffTime = 1.) :
            releaseDuration (std::max(0., releaseDuration))
        ,   voiceIntervals (columns > 2 && this->releaseDuration > 0.
                    ? std::clamp((size_t) std::llround((double) (columns - 1)
                            / (1. + this->releaseDuration)), size_t(1), columns - 2)
                    : std::max(size_t(2), columns) - 1)
        ,   tailIntervals (columns > voiceIntervals + 1 ? columns - 1 - voiceIntervals : 0)
        ,   releaseColumn ((size_t) std::llround(std::clamp(noteOffTime, 0., 1.)
                    * (double) voiceIntervals)) {}

    double increment(size_t column = 0) const {
        return column >= voiceIntervals && tailIntervals > 0
                ? releaseDuration / (double) tailIntervals
                : 1. / (double) voiceIntervals;
    }
    float morphPosition(size_t column) const {
        return (float) std::min(column, releaseColumn) / (float) voiceIntervals;
    }
    float noteOffPosition() const {
        return (float) releaseColumn / (float) (voiceIntervals + tailIntervals);
    }
    bool beginsRelease(size_t column) const {
        return column == releaseColumn
                && (releaseColumn < voiceIntervals || (releaseDuration > 0. && tailIntervals > 0));
    }

private:
    double releaseDuration;
    size_t voiceIntervals;
    size_t tailIntervals;
    size_t releaseColumn;
};

}

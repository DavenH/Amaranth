#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace CycleV2 {

// A common voice-time axis, with note-off at 1 and enough room for the longest tail.
class PreviewTraversal {
public:
    explicit PreviewTraversal(size_t columns, double releaseDuration = 0.) :
            releaseDuration (std::max(0., releaseDuration))
        ,   sustainIntervals (columns > 2 && this->releaseDuration > 0.
                    ? std::clamp((size_t) std::llround((double) (columns - 1)
                            / (1. + this->releaseDuration)), size_t(1), columns - 2)
                    : std::max(size_t(2), columns) - 1)
        ,   releaseIntervals (columns > sustainIntervals + 1 ? columns - 1 - sustainIntervals : 0) {}

    double increment(size_t column = 0) const {
        return column >= sustainIntervals && releaseIntervals > 0
                ? releaseDuration / (double) releaseIntervals
                : 1. / (double) sustainIntervals;
    }
    float morphPosition(size_t column) const {
        return (float) std::min(1., (double) column / (double) sustainIntervals);
    }
    float noteOffPosition() const {
        return (float) sustainIntervals / (float) (sustainIntervals + releaseIntervals);
    }
    bool beginsRelease(size_t column) const {
        return releaseDuration > 0. && releaseIntervals > 0 && column == sustainIntervals;
    }

private:
    double releaseDuration;
    size_t sustainIntervals;
    size_t releaseIntervals;
};

}

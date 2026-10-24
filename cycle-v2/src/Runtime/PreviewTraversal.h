#pragma once

#include <algorithm>
#include <cstddef>

namespace CycleV2 {

// Spy traversal: [0,1] before note-off, then one quarter of that duration in release.
class PreviewTraversal {
public:
    explicit PreviewTraversal(size_t columns) :
            sustainIntervals (columns > 2
                    ? std::max(size_t(1), (columns - 1) * 4 / 5)
                    : std::max(size_t(1), columns - 1)) {}

    double increment() const { return 1.0 / (double) sustainIntervals; }
    float morphPosition(size_t column) const {
        return (float) std::min(1.0, (double) column * increment());
    }
    bool beginsRelease(size_t column) const { return column == sustainIntervals + 1; }

private:
    size_t sustainIntervals;
};

}

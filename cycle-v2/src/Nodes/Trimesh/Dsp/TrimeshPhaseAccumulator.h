#pragma once

#include <Array/Buffer.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace CycleV2 {

class TrimeshPhaseAccumulator {
public:
    void prepare(size_t maximumBinCount) {
        phase.resize(maximumBinCount);
        reset();
    }

    void reset() {
        std::fill(phase.begin(), phase.end(), 0.f);
    }

    void integrate(Buffer<float> radiansPerSecond, double elapsedSeconds) {
        const int count = std::min(radiansPerSecond.size(), (int) phase.size());
        Buffer<float> running(phase.data(), count);
        running.addProduct(radiansPerSecond.withSize(count), (float) elapsedSeconds);

        // VecOps has no phase-modulo operation. Keep each bin bounded before
        // exposing the accumulated phase to the graph.
        constexpr float turn = MathConstants<float>::twoPi;
        for (int bin = 0; bin < count; ++bin) {
            phase[(size_t) bin] = std::remainder(phase[(size_t) bin], turn);
        }
        running.copyTo(radiansPerSecond.withSize(count));
        if (count < radiansPerSecond.size()) {
            radiansPerSecond.offset(count).zero();
        }
    }

    void copyCurrent(Buffer<float> destination) {
        const int count = std::min(destination.size(), (int) phase.size());
        Buffer<float>(phase.data(), count)
                .copyTo(destination.withSize(count));
        if (count < destination.size()) {
            destination.offset(count).zero();
        }
    }

private:
    std::vector<float> phase;
};

}

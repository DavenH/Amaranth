#pragma once

#include <Array/VecOps.h>

#include <algorithm>
#include <vector>

namespace CycleV2 {

class TrimeshPhaseAccumulator {
public:
    void prepare(size_t maximumBinCount) {
        phase.resize(maximumBinCount);
        wholeTurns.resize(maximumBinCount);
        fractionalTurns.resize(maximumBinCount);
        reset();
    }

    void reset() {
        std::fill(phase.begin(), phase.end(), 0.f);
    }

    void integrate(Buffer<float> radiansPerSecond, double elapsedSeconds) {
        const int count = std::min(radiansPerSecond.size(), (int) phase.size());
        Buffer<float> running(phase.data(), count);
        running.addProduct(radiansPerSecond.withSize(count), (float) elapsedSeconds);

        // Split turns around the half-turn boundary to keep phase in [-pi, pi).
        constexpr float turn = MathConstants<float>::twoPi;
        running.add(MathConstants<float>::pi).div(turn);
        Buffer<float> whole(wholeTurns.data(), count);
        Buffer<float> fraction(fractionalTurns.data(), count);
        VecOps::splitFrac(running, whole, fraction);
        fraction.sub(0.5f).mul(turn).copyTo(running);
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
    std::vector<float> wholeTurns;
    std::vector<float> fractionalTurns;
};

}

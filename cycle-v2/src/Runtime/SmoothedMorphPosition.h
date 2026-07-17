#pragma once

#include <Obj/MorphPosition.h>

#include <cmath>

namespace CycleV2 {

class SmoothedMorphPosition {
public:
    SmoothedMorphPosition() = default;

    explicit SmoothedMorphPosition(const MorphPosition& initial) {
        reset(initial);
    }

    void reset(const MorphPosition& value) {
        position.time.setValueDirect(value.time.getCurrentValue());
        position.red.setValueDirect(value.red.getCurrentValue());
        position.blue.setValueDirect(value.blue.getCurrentValue());
        sampleRemainder = 0.0;
    }

    void setTargets(const MorphPosition& targets) {
        position.time.setTargetValue(targets.time.getCurrentValue());
        position.red.setTargetValue(targets.red.getCurrentValue());
        position.blue.setTargetValue(targets.blue.getCurrentValue());
    }

    int advance(size_t frameCount, double sampleRate) {
        if (frameCount == 0 || sampleRate <= 0.0) {
            return 0;
        }

        const double exactSamples = sampleRemainder
                + (double) frameCount * referenceSampleRate / sampleRate;
        const int elapsedSamples = (int) std::floor(exactSamples);
        sampleRemainder = exactSamples - elapsedSamples;
        if (elapsedSamples > 0) {
            position.update(elapsedSamples);
        }
        return elapsedSamples;
    }

    const MorphPosition& current() const { return position; }

private:
    static constexpr double referenceSampleRate = 44100.0;

    MorphPosition position;
    double sampleRemainder {};
};

}

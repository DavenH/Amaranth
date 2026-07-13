#pragma once

#include <Array/Buffer.h>

#include <cstddef>
#include <vector>

namespace CycleDsp {

double delayTimeSeconds(double unitValue, double bpm, int beatsPerMeasure);
int delaySpinIterations(double unitValue);

enum class DelayChannel {
    Left,
    Right
};

struct DelayConfiguration {
    double sampleRate { 44100.0 };
    double delaySeconds { 0.5 };
    float feedback { 0.5f };
    float spin { 1.f };
    float wet { 0.9f };
    int spinIterations { 1 };
    DelayChannel channel { DelayChannel::Left };
};

class CycleDelay {
public:
    void configure(const DelayConfiguration& configuration);
    void reset();
    void process(Buffer<float> buffer);

private:
    struct SpinState {
        float pan {};
        float startingLevel {};
        size_t inputDelaySamples {};
        size_t spinDelaySamples {};
        std::vector<float> buffer;
    };

    static bool configurationsMatch(
            const DelayConfiguration& first,
            const DelayConfiguration& second);

    DelayConfiguration configuration;
    std::vector<float> inputBuffer;
    std::vector<SpinState> spinStates;
    size_t readPosition {};
    bool configured {};
};

}

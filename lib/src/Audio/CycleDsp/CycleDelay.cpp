#include "CycleDelay.h"

#include <Util/NumberUtils.h>

#include <algorithm>
#include <cmath>

namespace CycleDsp {

double delayTimeSeconds(double unitValue, double bpm, int beatsPerMeasure) {
    const double normalizedValue = std::max(0.15, unitValue);
    const double normalizedBpm = std::max(1.0, bpm);
    const int normalizedBeats = std::max(1, beatsPerMeasure);
    return normalizedBeats * normalizedValue * normalizedValue * 60.0 / normalizedBpm;
}

int delaySpinIterations(double unitValue) {
    return std::max(1, (int) (12.0 * unitValue * unitValue));
}

void CycleDelay::configure(const DelayConfiguration& configurationToUse) {
    DelayConfiguration normalized = configurationToUse;
    normalized.sampleRate = std::max(1.0, normalized.sampleRate);
    normalized.delaySeconds = std::max(0.0, normalized.delaySeconds);
    normalized.spinIterations = std::max(1, normalized.spinIterations);

    if (configured && configurationsMatch(configuration, normalized)) {
        return;
    }

    configuration = normalized;

    const int singleSize = std::max(
            1,
            (int) (configuration.spinIterations
                    * configuration.delaySeconds
                    * configuration.sampleRate));
    const size_t bufferSize = std::max<size_t>(
            2,
            (size_t) NumberUtils::nextPower2((unsigned) singleSize + 1));

    inputBuffer.assign(bufferSize, 0.f);
    spinStates.resize((size_t) configuration.spinIterations);
    readPosition = 0;

    for (int spinIndex = 0; spinIndex < configuration.spinIterations; ++spinIndex) {
        auto& state = spinStates[(size_t) spinIndex];
        const float pan = 0.5f + 0.49999f
                * configuration.spin
                * std::sin(spinIndex
                        / (float) configuration.spinIterations
                        * MathConstants<float>::twoPi);
        state.pan = configuration.channel == DelayChannel::Left
                ? std::min(1.f, 2.f * (1.f - pan))
                : std::min(1.f, 2.f * pan);
        state.startingLevel = std::pow(configuration.feedback, spinIndex + 1);
        state.inputDelaySamples = (size_t) std::max(
                0,
                (int) ((spinIndex + 1)
                        * configuration.delaySeconds
                        * configuration.sampleRate));
        state.spinDelaySamples = (size_t) std::max(
                0,
                (int) (configuration.spinIterations
                        * configuration.delaySeconds
                        * configuration.sampleRate));
        state.buffer.assign(bufferSize, 0.f);
    }

    configured = true;
}

void CycleDelay::reset() {
    std::fill(inputBuffer.begin(), inputBuffer.end(), 0.f);
    for (auto& state : spinStates) {
        std::fill(state.buffer.begin(), state.buffer.end(), 0.f);
    }
    readPosition = 0;
}

void CycleDelay::process(Buffer<float> buffer) {
    if (!configured || buffer.empty() || inputBuffer.empty() || spinStates.empty()) {
        return;
    }

    const size_t inputMask = inputBuffer.size() - 1;
    const size_t spinMask = spinStates.front().buffer.size() - 1;
    const float feedbackPower = std::pow(
            configuration.feedback,
            (int) spinStates.size()) + 1e-17f;

    for (int sampleIndex = 0; sampleIndex < buffer.size(); ++sampleIndex) {
        const float input = buffer[sampleIndex];
        float wetSum = 0.f;
        const size_t writePosition = readPosition & spinMask;

        inputBuffer[readPosition & inputMask] = input;

        for (auto& state : spinStates) {
            const size_t inputPosition = (readPosition - state.inputDelaySamples) & inputMask;
            const size_t feedbackPosition = (readPosition - state.spinDelaySamples) & spinMask;
            const float delayedFeedback = feedbackPower * state.buffer[feedbackPosition] + 1e-17f;

            state.buffer[writePosition] = inputBuffer[inputPosition] + delayedFeedback;
            wetSum += state.pan * state.startingLevel * state.buffer[writePosition];
        }

        buffer[sampleIndex] = input + configuration.wet * wetSum + 1e-19f;
        ++readPosition;
    }
}

bool CycleDelay::configurationsMatch(
        const DelayConfiguration& first,
        const DelayConfiguration& second) {
    return first.sampleRate == second.sampleRate
            && first.delaySeconds == second.delaySeconds
            && first.feedback == second.feedback
            && first.spin == second.spin
            && first.wet == second.wet
            && first.spinIterations == second.spinIterations
            && first.channel == second.channel;
}

}

#include "UnisonCore.h"

#include <algorithm>
#include <cmath>

namespace CycleDsp {

namespace {

constexpr double kJitters[maximumUnisonOrder - 1][maximumUnisonOrder] {
        {  0.001, -0.001 },
        { -0.108, -0.061,  0.163 },
        {  0.023, -0.067, -0.124,  0.168 },
        { -0.135,  0.094,  0.022, -0.122,  0.143 },
        { -0.076, -0.111,  0.123, -0.093,  0.178, -0.062 },
        {  0.061, -0.066,  0.056, -0.083,  0.215,  0.123, -0.018 },
        { -0.013, -0.061,  0.115,  0.231, -0.024, -0.172,  0.156,  0.155 },
        {  0.023, -0.065, -0.128,  0.169, -0.127,  0.149, -0.103, -0.062,  0.163 },
        { -0.068,  0.054, -0.086,  0.212,  0.126, -0.014,  0.023, -0.063, -0.128, 0.163 }
};

int constrainedOrder(int order) {
    return std::clamp(order, 1, maximumUnisonOrder);
}

}

double UnisonPhaseTrajectory::phaseAt(double seconds) const {
    return UnisonCore::wrapSignedPhase(
            (double) initialPhaseCycles + seconds * driftCyclesPerSecond);
}

int UnisonCore::orderFromUnitValue(double unitValue) {
    return std::min(maximumUnisonOrder, (int) (maximumUnisonOrder * unitValue + 1.0));
}

float UnisonCore::detuneCentsFromPosition(float position, float widthCents) {
    return widthCents * (2.f * position - 1.f);
}

float UnisonCore::voiceLevelScale(int order) {
    return std::pow(2.f, -(float) (std::max(order, 1) - 1) * 0.14f);
}

UnisonVoiceLayout UnisonCore::makeGroupLayout(
        const UnisonGroupConfiguration& configuration) {
    UnisonVoiceLayout layout;
    layout.order = configuration.enabled ? constrainedOrder(configuration.order) : 1;
    if (layout.order == 1) {
        return layout;
    }

    const float tuningIncrement = 1.f / (float) (layout.order - 1);
    const float phaseIncrement = 1.f / (float) layout.order;
    const int jitterIndex = layout.order - 2;
    for (int index = 0; index < layout.order; ++index) {
        UnisonVoice& voice = layout.voices[(size_t) index];
        const float jitter = (float) kJitters[jitterIndex][index];
        const float bipolarPosition = 2.f * tuningIncrement * (float) index - 1.f
                - configuration.jitter * jitter;
        voice.detunePosition = 0.5f * (bipolarPosition + 1.f);
        voice.detuneCents = configuration.detuneWidthCents * bipolarPosition;
        const float alternatingSide = index % 2 == 0 ? 1.f : -1.f;
        if (layout.order % 2 == 0 || index < layout.order / 2) {
            voice.pan = 0.5f + configuration.panSpread * 0.5f * alternatingSide;
        } else {
            voice.pan = 0.5f - configuration.panSpread * 0.5f * alternatingSide;
            if (index == layout.order / 2) {
                voice.pan = 0.5f;
            }
        }

        voice.phaseCycles = configuration.phaseSpread
                * (((float) index - (float) (layout.order - 1) * 0.5f) * phaseIncrement
                        + configuration.jitter * jitter);
        if (voice.phaseCycles < 0.f) {
            voice.phaseCycles += 1.f;
        }
    }
    return layout;
}

UnisonPhaseTrajectory UnisonCore::phaseTrajectory(
        int midiNote,
        float detuneCents,
        float initialPhaseCycles) {
    const double baseFrequency = frequencyForMidiNote(midiNote);
    return {
            initialPhaseCycles,
            frequencyForMidiNote(midiNote, detuneCents) - baseFrequency
    };
}

double UnisonCore::frequencyForMidiNote(int midiNote, float detuneCents) {
    constexpr int midiA = 69;
    return 440.0 * std::pow(
            2.0,
            ((double) midiNote + (double) detuneCents * 0.01 - midiA) / 12.0);
}

double UnisonCore::wrapSignedPhase(double cycles) {
    return cycles - std::floor(cycles + 0.5);
}

}

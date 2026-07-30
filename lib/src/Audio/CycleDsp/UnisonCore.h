#pragma once

#include <array>
#include <cstddef>

namespace CycleDsp {

constexpr int maximumUnisonOrder = 10;
constexpr float maximumUnisonDetuneCents = 70.f;

struct UnisonGroupConfiguration {
    int order { 1 };
    float detuneWidthCents { maximumUnisonDetuneCents * 0.5f };
    float panSpread { 1.f };
    float phaseSpread { 0.5f };
    float jitter { 0.5f };
    bool enabled { true };
};

struct UnisonVoice {
    float detunePosition { 0.5f };
    float detuneCents {};
    float pan { 0.5f };
    float phaseCycles {};
};

struct UnisonVoiceLayout {
    int order { 1 };
    std::array<UnisonVoice, maximumUnisonOrder> voices {};

    const UnisonVoice& operator[](int index) const { return voices[(size_t) index]; }
};

struct UnisonPhaseTrajectory {
    float initialPhaseCycles {};
    double driftCyclesPerSecond {};

    double phaseAt(double seconds) const;
};

class UnisonCore {
public:
    static int orderFromUnitValue(double unitValue);
    static float detuneCentsFromPosition(float position, float widthCents);
    static float voiceLevelScale(int order);

    static UnisonVoiceLayout makeGroupLayout(const UnisonGroupConfiguration& configuration);
    static UnisonPhaseTrajectory phaseTrajectory(
            int midiNote,
            float detuneCents,
            float initialPhaseCycles);

    static double frequencyForMidiNote(int midiNote, float detuneCents = 0.f);
    static double wrapSignedPhase(double cycles);
};

}

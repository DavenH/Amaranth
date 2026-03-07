#pragma once

struct LinearPhasePolicy {
    double phase;
    double delta;

    LinearPhasePolicy(double phase, double delta) noexcept :
            phase(phase)
        ,   delta(delta)
    {}

    double currentPhase() const noexcept { return phase; }
    void advance() noexcept { phase += delta; }
    void finalize() noexcept {}
};

struct VoiceFoldPhasePolicy {
    double phase;
    double delta;

    VoiceFoldPhasePolicy(double phase, double delta) noexcept :
            phase(phase)
        ,   delta(delta)
    {}

    double currentPhase() const noexcept { return phase; }
    void advance() noexcept { phase += delta; }

    void finalize() noexcept {
        if (phase > 0.5) {
            phase -= 1.0;
        }
    }
};

struct LoopingPhasePolicy {
    double phase;
    double delta;
    double loopStart;
    double loopEnd;
    double loopLength;
    bool loopEnabled;

    LoopingPhasePolicy(
        double phase,
        double delta,
        double loopStart,
        double loopEnd,
        bool loopEnabled) noexcept :
            phase(phase)
        ,   delta(delta)
        ,   loopStart(loopStart)
        ,   loopEnd(loopEnd)
        ,   loopLength(loopEnd - loopStart)
        ,   loopEnabled(loopEnabled)
    {}

    double currentPhase() const noexcept {
        if (! loopEnabled) {
            return phase;
        }

        double p = phase;
        while (p >= loopEnd) { p -= loopLength; }
        while (p < loopStart) { p += loopLength; }
        return p;
    }

    void advance() noexcept {
        double p = currentPhase();
        phase = p + delta;
        if (loopEnabled) {
            while (phase >= loopEnd) { phase -= loopLength; }
        }
    }

    void finalize() noexcept {}
};

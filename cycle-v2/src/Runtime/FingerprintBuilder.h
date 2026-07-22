#pragma once

#include <cstdint>

#include <juce_core/juce_core.h>

namespace CycleV2 {

inline uint64_t combineFingerprint(uint64_t seed, uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    return seed;
}

class FingerprintBuilder {
public:
    explicit FingerprintBuilder(uint64_t seed = 1469598103934665603ULL) : state(seed) {}

    FingerprintBuilder& add(uint64_t value) {
        state = combineFingerprint(state, value);
        return *this;
    }

    FingerprintBuilder& add(const juce::String& value) {
        return add(static_cast<uint64_t>(value.hashCode64()));
    }

    uint64_t value() const { return state; }

private:
    uint64_t state;
};

}

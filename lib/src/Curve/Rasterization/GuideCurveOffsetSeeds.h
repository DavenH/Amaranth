#pragma once

#include <algorithm>
#include <cstdint>

namespace Rasterization {
    struct GuideCurveSeed {
        enum class Scope {
            Visualization,
            VoiceLifecycle
        };

        static GuideCurveSeed visualization(uint32_t stableIdentity) {
            return { stableIdentity, Scope::Visualization };
        }

        static GuideCurveSeed voiceLifecycle(uint32_t lifecycleSeed) {
            return { lifecycleSeed, Scope::VoiceLifecycle };
        }

        uint32_t value {};
        Scope scope { Scope::Visualization };
    };

    struct GuideCurveOffsetSeeds {
        static constexpr int capacity = 128;

        void reset() {
            std::fill_n(phaseSeeds, capacity, short());
            std::fill_n(verticalSeeds, capacity, short());
        }

        void derive(int count, int tableSize, GuideCurveSeed seed) {
            reset();
            if (tableSize <= 0) {
                return;
            }

            uint32_t state = seed.value;
            const int limit = std::min(count, capacity);
            for (int i = 0; i < limit; ++i) {
                verticalSeeds[i] = short(next(state) % uint32_t(tableSize));
                phaseSeeds[i] = short(next(state) % uint32_t(tableSize));
            }
        }

        const short* phase() const    { return phaseSeeds;     }
        const short* vertical() const { return verticalSeeds;  }

        short phaseAt(int index) const {
            return isInRange(index) ? phaseSeeds[index] : short();
        }

        short verticalAt(int index) const {
            return isInRange(index) ? verticalSeeds[index] : short();
        }

        short phaseSeeds[capacity] {};
        short verticalSeeds[capacity] {};

    private:
        static uint32_t next(uint32_t& state) {
            state += 0x9e3779b9u;
            uint32_t value = state;
            value = (value ^ (value >> 16u)) * 0x21f0aaadu;
            value = (value ^ (value >> 15u)) * 0x735a2d97u;
            return value ^ (value >> 15u);
        }

        static bool isInRange(int index) {
            return index >= 0 && index < capacity;
        }
    };
}

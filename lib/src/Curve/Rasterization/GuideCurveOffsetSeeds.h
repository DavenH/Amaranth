#pragma once

#include <algorithm>

namespace Rasterization {
    struct GuideCurveOffsetSeeds {
        static constexpr int capacity = 128;

        void reset() {
            std::fill(phaseSeeds, phaseSeeds + capacity, short());
            std::fill(verticalSeeds, verticalSeeds + capacity, short());
        }

        template<typename RandomSource>
        void randomize(int count, int tableSize, RandomSource& random) {
            int limit = std::min(count, capacity);

            for (int i = 0; i < limit; ++i) {
                verticalSeeds[i] = short(random.nextInt(tableSize));
                phaseSeeds[i]    = short(random.nextInt(tableSize));
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
        static bool isInRange(int index) {
            return index >= 0 && index < capacity;
        }
    };
}

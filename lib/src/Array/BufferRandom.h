#pragma once

#include <random>

namespace BufferRandom {

inline void fill(float* destination, int size, unsigned& seed) {
    std::mt19937 generator(seed);
    constexpr double divisor = (double) std::mt19937::max() + 1.0;

    for (int index = 0; index < size; ++index) {
        destination[index] = (float) ((double) generator() / divisor);
    }

    seed = generator();
}

}

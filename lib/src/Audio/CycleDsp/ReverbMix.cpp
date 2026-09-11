#include "Audio/CycleDsp/ReverbMix.h"

#include <Array/VecOps.h>

namespace CycleDsp {

void mixReverbMono(
        Buffer<float> dry,
        Buffer<float> wet,
        Buffer<float> output,
        float wetLevel) {
    VecOps::mul(wet, wetLevel, output);
    output.addProduct(dry, 1.f - 0.25f * wetLevel);
}

void mixReverbChannel(
        Buffer<float> dry,
        Buffer<float> directWet,
        Buffer<float> crossWet,
        Buffer<float> output,
        float wetLevel,
        float width) {
    const float direct = wetLevel * jmax(0.5f, width);
    const float cross = wetLevel * jmin(0.5f, 1.f - width);
    const float dryScale = 1.f - 0.24f * wetLevel;

    VecOps::mul(directWet, direct, output);
    output.addProduct(crossWet, cross);
    output.addProduct(dry, dryScale);
}

}

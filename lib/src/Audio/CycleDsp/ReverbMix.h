#pragma once

#include <Array/Buffer.h>

namespace CycleDsp {

void mixReverbMono(
        Buffer<float> dry,
        Buffer<float> wet,
        Buffer<float> output,
        float wetLevel);

void mixReverbChannel(
        Buffer<float> dry,
        Buffer<float> directWet,
        Buffer<float> crossWet,
        Buffer<float> output,
        float wetLevel,
        float width);

}

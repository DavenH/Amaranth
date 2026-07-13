#pragma once

#include <Array/Buffer.h>

namespace CycleDsp {

struct ReverbKernelConfiguration {
    float roomSize { 0.2f };
    float damping { 0.35f };
    float highPass { 0.05f };
};

void buildReverbKernel(
        const ReverbKernelConfiguration& configuration,
        Buffer<float> left,
        Buffer<float> right = {});

}

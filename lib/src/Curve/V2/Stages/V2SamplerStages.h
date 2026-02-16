#pragma once

#include "V2StageInterfaces.h"

class V2LinearSamplerStage :
        public V2SamplerStage {
public:
    V2RenderResult run(
        Buffer<float> waveX,
        Buffer<float> waveY,
        Buffer<float> slope,
        Buffer<float> output,
        const V2SamplerContext& context) noexcept override;
};


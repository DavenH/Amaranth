#pragma once

#include "V2StageInterfaces.h"

class V2LinearSamplerStage :
        public V2SamplerStage {
public:
    V2RenderResult run(
            const V2WaveBuffers& waveBuffers,
            Buffer<float> output,
            const V2SamplerContext& context) noexcept override;
};

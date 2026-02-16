#pragma once

#include "V2StageInterfaces.h"

class V2DefaultWaveBuilderStage :
        public V2WaveBuilderStage {
public:
    bool run(
        const std::vector<Curve>& curves,
        int curveCount,
        Buffer<float> waveX,
        Buffer<float> waveY,
        Buffer<float> diffX,
        Buffer<float> slope,
        int& outWavePointCount,
        int& zeroIndex,
        int& oneIndex,
        const V2WaveBuilderContext& context) noexcept override;
};


#pragma once

#include "V2StageInterfaces.h"

class V2DefaultCurveBuilderStage :
        public V2CurveBuilderStage {
public:
    bool run(
        const std::vector<Intercept>& inIntercepts,
        int inCount,
        std::vector<Curve>& outCurves,
        int& outCount,
        const V2CurveBuilderContext& context) noexcept override;
};


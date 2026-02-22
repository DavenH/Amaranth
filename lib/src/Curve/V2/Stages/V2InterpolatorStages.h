#pragma once

#include "V2StageInterfaces.h"

class V2TrilinearInterpolatorStage :
        public V2InterpolatorStage {
public:
    bool run(
        const V2InterpolatorContext& context,
        std::vector<Intercept>& outIntercepts,
        int& outCount) noexcept override;
};

class V2BilinearInterpolatorStage :
        public V2InterpolatorStage {
public:
    bool run(
        const V2InterpolatorContext& context,
        std::vector<Intercept>& outIntercepts,
        int& outCount) noexcept override;
};

class V2SimpleInterpolatorStage :
        public V2InterpolatorStage {
public:
    bool run(
        const V2InterpolatorContext& context,
        std::vector<Intercept>& outIntercepts,
        int& outCount) noexcept override;
};

class V2FxVertexInterpolatorStage :
        public V2InterpolatorStage {
public:
    bool run(
        const V2InterpolatorContext& context,
        std::vector<Intercept>& outIntercepts,
        int& outCount) noexcept override;
};

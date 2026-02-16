#pragma once

#include "V2StageInterfaces.h"

class V2LinearPositionerStage :
        public V2PositionerStage {
public:
    bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept override;
};

class V2CyclicPositionerStage :
        public V2PositionerStage {
public:
    bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept override;
};


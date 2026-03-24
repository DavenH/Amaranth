#pragma once

#include "V2StageInterfaces.h"

void applyResolutionPolicy(std::vector<Curve>& curves, const V2CurveBuilderContext& context);

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

class V2VoiceChainingCurveBuilderStage :
        public V2CurveBuilderStage {
public:
    void reset() noexcept;
    void prime(const std::vector<Intercept>& intercepts) noexcept;

    bool run(
        const std::vector<Intercept>& inIntercepts,
        int inCount,
        std::vector<Curve>& outCurves,
        int& outCount,
        const V2CurveBuilderContext& context) noexcept override;

private:
    Intercept frontA{-0.05f, 0.0f};
    Intercept frontB{-0.10f, 0.0f};
    Intercept frontC{-0.15f, 0.0f};
    Intercept frontD{-0.25f, 0.0f};
    Intercept frontE{-0.35f, 0.0f};
    std::vector<Intercept> previousIntercepts;
};

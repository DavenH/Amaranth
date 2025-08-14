#pragma once

enum ScalingType { Unipolar, Bipolar, HalfBipolar };

struct CurveParameters {
    bool cyclic{false};
    bool interpolate{true};
    float padding{0.0f};
    float minX{0.0f};
    float maxX{1.0f};
};

struct SamplingParameters {
    bool integralSampling{false};
    bool lowResolution{false};
    int oversamplingFactor{1};
};

struct InterpolationParameters {
    bool useLinear{false};
    float smoothingFactor{1.0f};
};

struct RasterizerParameters {
    CurveParameters curve;
    SamplingParameters sampling;
    InterpolationParameters interpolation;
    ScalingType scaling{ScalingType::Unipolar};
};
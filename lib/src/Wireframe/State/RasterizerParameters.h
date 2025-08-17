#pragma once

enum ScalingType { Unipolar, Bipolar, HalfBipolar };

struct CurveParameters {
    bool cyclic{false};
    bool interpolate{true};
    float padding{0.0f};
    bool useMinMax{true};
    float minX{0.0f};
    float maxX{1.0f};
};


struct GeneratorParameters {
    ScalingType scaling {Unipolar};
    // Defaults to 2 in OldMeshRasterizer. FXRasterizer sets it to 1.
    int paddingCount{2};

    // base = some runtime size of the wavelength?
    float base{1.f};
};

struct SamplingParameters {
    bool integralSampling{false};
    bool lowResolution{false};
    int oversamplingFactor{1};
};

struct InterpolatorParameters {
    double oscPhase;
};

struct RasterizerParameters {
    CurveParameters curve;
    SamplingParameters sampling;
    GeneratorParameters generator;
    InterpolatorParameters interpolation{};
};
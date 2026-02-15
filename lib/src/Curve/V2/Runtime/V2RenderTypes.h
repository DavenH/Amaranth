#pragma once

enum class V2ScalingType {
    Unipolar = 0,
    Bipolar,
    HalfBipolar
};

struct V2CapacitySpec {
    int maxIntercepts{0};
    int maxCurves{0};
    int maxWavePoints{0};
    int maxDeformRegions{0};

    bool isValid() const noexcept {
        return maxIntercepts > 0
            && maxCurves > 0
            && maxWavePoints > 1
            && maxDeformRegions >= 0;
    }
};

struct V2PrepareSpec {
    V2CapacitySpec capacities;
    bool lowResolution{false};
    bool oneSamplePerCycle{false};
};

struct V2RenderRequest {
    int numSamples{0};
    int unisonIndex{0};
    double deltaX{0.0};
    float tempoScale{1.0f};
    int scale{1};
    bool tempoSync{false};
    bool logarithmic{false};

    bool isValid() const noexcept {
        return numSamples >= 0 && deltaX >= 0.0 && tempoScale > 0.0f && scale > 0;
    }
};

struct V2RenderResult {
    bool rendered{false};
    bool stillActive{false};
    int samplesWritten{0};
};


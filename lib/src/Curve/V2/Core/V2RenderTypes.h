#pragma once

#include <vector>

#include <Curve/GuideCurveProvider.h>
#include <Curve/Intercept.h>

enum class V2ScalingType {
    Unipolar,
    Bipolar,
    HalfBipolar
};

struct V2DeformRegion {
    int deformChan{0};
    float amplitude{0.0f};
    Intercept start{};
    Intercept end{};
};

struct V2DecoupledDeformContext {
    GuideCurveProvider* path{nullptr};
    const std::vector<V2DeformRegion>* deformRegions{nullptr};
    int noiseSeed{0};
    int phaseOffsetSeed{0};
    int vertOffsetSeed{0};

    bool isEnabled() const noexcept {
        return path != nullptr && deformRegions != nullptr && ! deformRegions->empty();
    }
};

struct V2WaveBuffers {
    Buffer<float> waveX;
    Buffer<float> waveY;
    Buffer<float> diffX;
    Buffer<float> slope;

    V2WaveBuffers() = default;

    V2WaveBuffers(
        Buffer<float> waveX,
        Buffer<float> waveY,
        Buffer<float> diffX,
        Buffer<float> slope) noexcept :
            waveX(waveX)
        ,   waveY(waveY)
        ,   diffX(diffX)
        ,   slope(slope)
    {}

    void clear() noexcept {
        waveX = Buffer<float>();
        waveY = Buffer<float>();
        diffX = Buffer<float>();
        slope = Buffer<float>();
    }

    bool hasStorage() const noexcept {
        return ! waveX.empty();
    }

    void zeroIfAllocated() noexcept {
        if (! hasStorage()) {
            return;
        }

        waveX.zero();
        waveY.zero();
        diffX.zero();
        slope.zero();
    }

    bool canContain(int wavePointCount) const noexcept {
        return wavePointCount > 1
            && wavePointCount <= waveX.size()
            && wavePointCount <= waveY.size();
    }

    void assignSized(V2WaveBuffers& out, int wavePointCount) const noexcept {
        const int slopeCount = jmax(0, wavePointCount - 1);
        out.waveX = waveX.withSize(wavePointCount);
        out.waveY = waveY.withSize(wavePointCount);
        out.diffX = diffX.withSize(slopeCount);
        out.slope = slope.withSize(slopeCount);
    }

    void copyToSized(V2WaveBuffers& out, int wavePointCount) const noexcept {
        waveX.withSize(wavePointCount).copyTo(out.waveX.withSize(wavePointCount));
        waveY.withSize(wavePointCount).copyTo(out.waveY.withSize(wavePointCount));

        const int slopeCount = jmax(0, wavePointCount - 1);
        if (slopeCount > 0) {
            diffX.withSize(slopeCount).copyTo(out.diffX.withSize(slopeCount));
            slope.withSize(slopeCount).copyTo(out.slope.withSize(slopeCount));
        }
    }
};

struct V2CapacitySpec {
    int maxIntercepts{256};
    int maxCurves{256};
    int maxWavePoints{4096};
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

struct V2GraphicRequest {
    int numSamples{0};
    bool interpolateCurves{true};
    bool lowResolution{false};

    bool isValid() const noexcept {
        return numSamples >= 0;
    }
};

struct V2GraphicResult {
    bool rendered{false};
    int pointsWritten{0};
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

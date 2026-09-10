#pragma once

#include <Array/Buffer.h>
#include <Util/Arithmetic.h>

#include <cmath>

namespace CycleDsp {

class SpectralLayerCore {
public:
    static void clearBinsAbove(
            Buffer<float> magnitudes,
            Buffer<float> phases,
            int activeBinCount) {
        const int magnitudeStart = jlimit(0, magnitudes.size(), activeBinCount);
        const int phaseStart = jlimit(0, phases.size(), activeBinCount);
        magnitudes.offset(magnitudeStart).zero();
        phases.offset(phaseStart).zero();
    }

    static float phaseOffsetScale(float range) {
        return expf(5.f * range);
    }

    static float rangeForPhaseOffsetScale(float scale) {
        return logf(scale) / 5.f;
    }

    static void preparePhaseHarmonicScale(Buffer<float> scale) {
        scale.ramp(1.f, 1.f).sqrt();
    }

    static float magnitudeDynamicRange(float range) {
        return sqrtf(powf(2.f, 12.f * range - 4.f));
    }

    static float magnitudeRangeScale(float range) {
        const float dynamicRange = magnitudeDynamicRange(range);
        return dynamicRange * dynamicRange;
    }

    static float rangeForMagnitudeScale(float scale) {
        return (log2f(scale) + 4.f) / 12.f;
    }

    static void shapeMagnitude(
            Buffer<float> values,
            float range,
            bool additive,
            int harmonicCount) {
        const float dynamicRange = magnitudeDynamicRange(range);
        const float threshold = powf(1.0e-19f, 1.f / dynamicRange);
        float scale = powf(2.f, dynamicRange);
        if (additive) {
            scale *= Arithmetic::calcAdditiveScaling(harmonicCount);
        }

        values.threshLT(threshold).pow(dynamicRange).mul(scale);
    }

    static void applyMultiplicativePan(Buffer<float> values, float channelGain) {
        if (channelGain < 1.f) {
            values.sub(1.f).mul(channelGain).add(1.f);
        }
    }

    static void renderMagnitudeChannels(
            Buffer<float> source,
            Buffer<float> left,
            Buffer<float> right,
            float pan,
            float range,
            bool additive) {
        float leftPan = 1.f;
        float rightPan = 1.f;
        Arithmetic::getPans(pan, leftPan, rightPan);

        source.copyTo(left);
        shapeMagnitude(left, range, additive, left.size());
        left.copyTo(right);
        if (additive) {
            left.mul(leftPan);
            right.mul(rightPan);
            return;
        }

        applyMultiplicativePan(left, leftPan);
        applyMultiplicativePan(right, rightPan);
    }

    static void renderPhaseChannels(
            Buffer<float> source,
            Buffer<float> left,
            Buffer<float> right,
            float pan,
            float range) {
        float leftPan = 1.f;
        float rightPan = 1.f;
        Arithmetic::getPans(pan, leftPan, rightPan);
        const float scale = phaseOffsetScale(range)
                * MathConstants<float>::twoPi;

        source.copyTo(left);
        source.copyTo(right);
        left.mul(scale * leftPan);
        right.mul(scale * rightPan);
    }
};

}

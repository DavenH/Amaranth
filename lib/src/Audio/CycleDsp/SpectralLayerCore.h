#pragma once

#include <Array/Buffer.h>
#include <Util/Arithmetic.h>

#include <cmath>

namespace CycleDsp {

class SpectralLayerCore {
public:
    static float phaseOffsetScale(float range) {
        return expf(5.f * range);
    }

    static float magnitudeDynamicRange(float range) {
        return sqrtf(powf(2.f, 12.f * range - 4.f));
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

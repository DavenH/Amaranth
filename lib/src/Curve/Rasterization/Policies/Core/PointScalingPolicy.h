#pragma once

namespace Rasterization {
    enum class PointScalingMode {
        Unipolar,
        Bipolar,
        HalfBipolar,
        Identity
    };

    class PointScalingPolicy {
    public:
        explicit PointScalingPolicy(PointScalingMode mode = PointScalingMode::Identity) :
                mode(mode) {
        }

        static PointScalingPolicy fromLegacyScalingType(int scalingType) {
            switch (scalingType) {
                case 0:     return PointScalingPolicy(PointScalingMode::Unipolar);
                case 1:     return PointScalingPolicy(PointScalingMode::Bipolar);
                case 2:     return PointScalingPolicy(PointScalingMode::HalfBipolar);
                default:    return PointScalingPolicy(PointScalingMode::Identity);
            }
        }

        static PointScalingPolicy fromLegacyFxScalingType(int scalingType) {
            return scalingType == 0
                 ? PointScalingPolicy(PointScalingMode::Unipolar)
                 : PointScalingPolicy(PointScalingMode::Bipolar);
        }

        float scale(float value) const {
            switch (mode) {
                case PointScalingMode::Unipolar:
                case PointScalingMode::Identity:
                    return value;

                case PointScalingMode::Bipolar:
                    return 2.f * value - 1.f;

                case PointScalingMode::HalfBipolar:
                    return value - 0.5f;
            }

            return value;
        }

        float minimum() const {
            switch (mode) {
                case PointScalingMode::Unipolar:
                case PointScalingMode::Identity:
                    return 0.f;

                case PointScalingMode::Bipolar:
                case PointScalingMode::HalfBipolar:
                    return -1.f;
            }

            return 0.f;
        }

        float maximum() const { return 1.f; }
        bool isBipolar() const { return mode == PointScalingMode::Bipolar || mode == PointScalingMode::HalfBipolar; }
        PointScalingMode getMode() const { return mode; }

    private:
        PointScalingMode mode;
    };
}

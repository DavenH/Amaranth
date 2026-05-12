#pragma once

#include <algorithm>

#include "../../../GuideCurveProvider.h"
#include "../../../Intercept.h"
#include "../../../VertCube.h"
#include "../../GuideCurveOffsetSeeds.h"
#include "../Core/PointScalingPolicy.h"
#include "../../../../Obj/MorphPosition.h"
#include "../../../../Util/NumberUtils.h"

namespace Rasterization {
    struct GuideCurvePolicyContext {
        GuideCurveProvider* guideCurveProvider {};
        VertCube::ReductionData* reduction {};
        PointScalingMode scalingMode { PointScalingMode::Unipolar };
        bool cyclic {};
        bool noOffsetAtEnds {};
        bool* needsResorting {};

        int noiseSeed {};
        const GuideCurveOffsetSeeds* offsetSeeds {};
    };

    class GuideCurvePolicy {
    public:
        explicit GuideCurvePolicy(const GuideCurvePolicyContext& context) :
                context(context) {
        }

        void apply(Intercept& intercept, const MorphPosition& morph) const {
            if (intercept.cube == nullptr || context.guideCurveProvider == nullptr || context.reduction == nullptr) {
                return;
            }

            VertCube* cube = intercept.cube;
            GuideCurveProvider::NoiseContext noise;
            noise.noiseSeed = context.noiseSeed;

            applyMorphAxisGuide(intercept, morph, noise, cube, Vertex::Red);
            applyMorphAxisGuide(intercept, morph, noise, cube, Vertex::Blue);
            applyOutputAxisGuides(intercept, noise, cube);
        }

    private:
        GuideCurvePolicyContext context;

        void applyMorphAxisGuide(
                Intercept& intercept,
                const MorphPosition& morph,
                GuideCurveProvider::NoiseContext& noise,
                VertCube* cube,
                int dimension) const {
            if (cube->guideCurveAt(dimension) < 0) {
                return;
            }

            int guideIndex = cube->guideCurveAt(dimension);
            float progress = cube->getPortionAlong(dimension, morph);
            applyNoiseOffsets(noise, guideIndex);

            intercept.adjustedX += cube->guideCurveAbsGain(dimension)
                    * context.guideCurveProvider->getTableValue(guideIndex, progress, noise);
        }

        void applyOutputAxisGuides(
                Intercept& intercept,
                GuideCurveProvider::NoiseContext& noise,
                VertCube* cube) const {
            float progress = getTimeProgress();
            bool ignore = context.noOffsetAtEnds && (progress == 0.f || progress == 1.f);

            applyAmplitudeGuide(intercept, noise, cube, progress, ignore);
            applyPhaseGuide(intercept, noise, cube, progress, ignore);
            applyCurveGuide(intercept, noise, cube, progress);
        }

        float getTimeProgress() const {
            float timeMin = context.reduction->v0.values[Vertex::Time];
            float timeMax = context.reduction->v1.values[Vertex::Time];

            if (timeMin > timeMax) {
                std::swap(timeMin, timeMax);
            }

            float diffTime = timeMax - timeMin;
            if (diffTime == 0.f) {
                return 0.f;
            }

            float time = context.reduction->v.values[Vertex::Time];
            return time >= timeMin ? (time - timeMin) / diffTime : (timeMin - time) / diffTime;
        }

        void applyAmplitudeGuide(
                Intercept& intercept,
                GuideCurveProvider::NoiseContext& noise,
                VertCube* cube,
                float progress,
                bool ignore) const {
            if (ignore || cube->guideCurveAt(Vertex::Amp) < 0) {
                return;
            }

            int guideIndex = cube->guideCurveAt(Vertex::Amp);
            applyNoiseOffsets(noise, guideIndex);

            PointScalingPolicy scalingPolicy(context.scalingMode);
            intercept.y += cube->guideCurveAbsGain(Vertex::Amp)
                    * context.guideCurveProvider->getTableValue(guideIndex, progress, noise);
            NumberUtils::constrain(intercept.y, scalingPolicy.minimum(), scalingPolicy.maximum());
        }

        void applyPhaseGuide(
                Intercept& intercept,
                GuideCurveProvider::NoiseContext& noise,
                VertCube* cube,
                float progress,
                bool ignore) const {
            if (ignore || cube->guideCurveAt(Vertex::Phase) < 0) {
                return;
            }

            int guideIndex = cube->guideCurveAt(Vertex::Phase);
            applyNoiseOffsets(noise, guideIndex);

            intercept.adjustedX += cube->guideCurveAbsGain(Vertex::Phase)
                    * context.guideCurveProvider->getTableValue(guideIndex, progress, noise);

            wrapPhase(intercept);
        }

        void applyCurveGuide(
                Intercept& intercept,
                GuideCurveProvider::NoiseContext& noise,
                VertCube* cube,
                float progress) const {
            if (cube->guideCurveAt(Vertex::Curve) < 0) {
                return;
            }

            int guideIndex = cube->guideCurveAt(Vertex::Curve);
            applyNoiseOffsets(noise, guideIndex);

            intercept.shp += 2.f * cube->guideCurveAbsGain(Vertex::Curve)
                    * context.guideCurveProvider->getTableValue(guideIndex, progress, noise);
            NumberUtils::constrain(intercept.shp, 0.f, 1.f);
        }

        void wrapPhase(Intercept& intercept) const {
            if (! context.cyclic) {
                return;
            }

            float lastAdjustedX = intercept.adjustedX;

            while (intercept.adjustedX >= 1.f) {
                intercept.adjustedX -= 1.f;
            }

            while (intercept.adjustedX < 0.f) {
                intercept.adjustedX += 1.f;
            }

            if (lastAdjustedX != intercept.adjustedX) {
                intercept.isWrapped = true;

                if (context.needsResorting != nullptr) {
                    *context.needsResorting = true;
                }
            }
        }

        void applyNoiseOffsets(GuideCurveProvider::NoiseContext& noise, int guideIndex) const {
            if (context.offsetSeeds != nullptr) {
                noise.vertOffset = context.offsetSeeds->verticalAt(guideIndex);
                noise.phaseOffset = context.offsetSeeds->phaseAt(guideIndex);
            }
        }
    };

    class GuideCurveApplier {
    public:
        explicit GuideCurveApplier(const GuideCurvePolicyContext& context) :
                context(context) {
        }

        void operator()(Intercept& intercept, const MorphPosition& morph) {
            apply(intercept, morph, false);
        }

        void operator()(Intercept& intercept, const MorphPosition& morph, bool noOffsetAtEnds) {
            apply(intercept, morph, noOffsetAtEnds);
        }

    private:
        GuideCurvePolicyContext context;

        void apply(Intercept& intercept, const MorphPosition& morph, bool noOffsetAtEnds) {
            context.noOffsetAtEnds = noOffsetAtEnds;
            GuideCurvePolicy(context).apply(intercept, morph);
        }
    };
}

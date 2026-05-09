#pragma once

#include <Curve/MeshRasterizer.h>

class GuideCurveProvider;
class Mesh;

namespace Cycle::Rasterization {
    class SpectralFilterRasterizer {
    public:
        explicit SpectralFilterRasterizer(const String& name = String()) :
                rasterizer(name) {
        }

        void configureMagnitude(GuideCurveProvider* guideCurveProvider, float spectralMargin) {
            configure(guideCurveProvider, spectralMargin);
        }

        void configurePhase(GuideCurveProvider* guideCurveProvider, float spectralMargin) {
            configure(guideCurveProvider, spectralMargin);
            rasterizer.setScalingMode(MeshRasterizer::Bipolar);
            rasterizer.setInterpolatesCurves(true);
        }

        void updateOffsetSeeds(int layerSize, int tableSize) {
            rasterizer.updateOffsetSeeds(layerSize, tableSize);
        }

        void setMorphPosition(const MorphPosition& morphPosition) {
            rasterizer.setMorphPosition(morphPosition);
        }

        void setNoiseSeed(int seed) {
            rasterizer.setNoiseSeed(seed);
        }

        void calcCrossPoints(Mesh* mesh) {
            rasterizer.calcCrossPoints(mesh, 0.f);
        }

        bool isSampleable() {
            return rasterizer.isSampleable();
        }

        void sampleAtIntervals(Buffer<float> intervals, Buffer<float> dest) {
            rasterizer.sampleAtIntervals(intervals, dest);
        }

    private:
        void configure(GuideCurveProvider* guideCurveProvider, float spectralMargin) {
            rasterizer.setWrapsEnds(false);
            rasterizer.setGuideCurveProvider(guideCurveProvider);
            rasterizer.setCalcDepthDimensions(false);
            rasterizer.setLimits(-spectralMargin, 1.f + spectralMargin);
        }

        MeshRasterizer rasterizer;
    };
}

#pragma once

#include <Curve/Rasterization/ComposedMeshWaveformRasterizer.h>

class GuideCurveProvider;
class Mesh;

namespace Cycle::Rasterization {
    class SpectralFilterRasterizer {
    public:
        SpectralFilterRasterizer() = default;

        void configureMagnitude(GuideCurveProvider* guideCurveProvider, float spectralMargin) {
            configure(guideCurveProvider, spectralMargin);
        }

        void configurePhase(GuideCurveProvider* guideCurveProvider, float spectralMargin) {
            configure(guideCurveProvider, spectralMargin);
            rasterizer.getRequest().scalingMode = ::Rasterization::PointScalingMode::Bipolar;
            rasterizer.getRequest().interpolateCurves = true;
        }

        void updateOffsetSeeds(int layerSize, int tableSize) {
            rasterizer.updateOffsetSeeds(layerSize, tableSize);
        }

        void setMorphPosition(const MorphPosition& morphPosition) {
            rasterizer.getRequest().morph = morphPosition;
        }

        void setNoiseSeed(int seed) {
            rasterizer.getRequest().noiseSeed = seed;
        }

        void calcCrossPoints(Mesh* mesh) {
            rasterizer.render(mesh);
        }

        bool isSampleable() {
            return rasterizer.isSampleable();
        }

        void sampleAtIntervals(Buffer<float> intervals, Buffer<float> dest) {
            rasterizer.sampleAtIntervals(intervals, dest);
        }

    private:
        void configure(GuideCurveProvider* guideCurveProvider, float spectralMargin) {
            rasterizer.setGuideCurveProvider(guideCurveProvider);
            rasterizer.getRequest().cyclic = false;
            rasterizer.getRequest().calcDepthDimensions = false;
            rasterizer.getRequest().xMinimum = -spectralMargin;
            rasterizer.getRequest().xMaximum = 1.f + spectralMargin;
        }

        ::Rasterization::ComposedMeshWaveformRasterizer rasterizer;
    };
}

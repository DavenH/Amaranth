#pragma once

#include "WaveformProvider.h"
#include "../../MeshRasterizer.h"

namespace Rasterization {
    class MeshRasterizerWaveformProviderAdapter :
            public WaveformProvider {
    public:
        MeshRasterizerWaveformProviderAdapter() = default;

        explicit MeshRasterizerWaveformProviderAdapter(MeshRasterizer* rasterizer) :
                rasterizer(rasterizer) {
        }

        void setRasterizer(MeshRasterizer* newRasterizer) {
            rasterizer = newRasterizer;
        }

        MeshRasterizer* getRasterizer() const { return rasterizer; }

        bool canRasterizeWaveform() const override {
            return rasterizer != nullptr && rasterizer->hasEnoughCubesForCrossSection();
        }

        bool isBipolar() const override {
            return rasterizer != nullptr && rasterizer->isBipolar();
        }

        void updateWaveform(UpdateType updateType) override {
            jassert(rasterizer != nullptr);

            if (rasterizer != nullptr) {
                rasterizer->performUpdate(updateType);
            }
        }

        double sampleWithInterval(Buffer<float> buffer, double delta, double phase) override {
            jassert(rasterizer != nullptr);
            return rasterizer != nullptr ? rasterizer->sampleWithInterval(buffer, delta, phase) : phase;
        }

        float samplePerfectly(double delta, Buffer<float> buffer, double phase) override {
            jassert(rasterizer != nullptr);
            return rasterizer != nullptr ? rasterizer->samplePerfectly(delta, buffer, phase) : 0.f;
        }

    private:
        MeshRasterizer* rasterizer {};
    };
}

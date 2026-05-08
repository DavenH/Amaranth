#pragma once

#include "RasterizerSampler.h"
#include "../../MeshRasterizer.h"

namespace Rasterization {
    class MeshRasterizerSamplerAdapter :
            public RasterizerSampler {
    public:
        MeshRasterizerSamplerAdapter() = default;

        explicit MeshRasterizerSamplerAdapter(MeshRasterizer* rasterizer) :
                rasterizer(rasterizer) {
        }

        void setRasterizer(MeshRasterizer* newRasterizer) {
            rasterizer = newRasterizer;
        }

        MeshRasterizer* getRasterizer() const { return rasterizer; }

        bool isSampleable() override {
            return rasterizer != nullptr && rasterizer->isSampleable();
        }

        bool isSampleableAt(float x) override {
            return rasterizer != nullptr && rasterizer->isSampleableAt(x);
        }

        float sampleAt(double angle) override {
            jassert(rasterizer != nullptr);
            return rasterizer != nullptr ? rasterizer->sampleAt(angle) : 0.5f;
        }

        float sampleAt(double angle, int& currentIndex) override {
            jassert(rasterizer != nullptr);
            return rasterizer != nullptr ? rasterizer->sampleAt(angle, currentIndex) : 0.5f;
        }

        float samplePerfectly(double delta, Buffer<float> buffer, double phase) override {
            jassert(rasterizer != nullptr);
            return rasterizer != nullptr ? rasterizer->samplePerfectly(delta, buffer, phase) : 0.f;
        }

    private:
        MeshRasterizer* rasterizer {};
    };
}

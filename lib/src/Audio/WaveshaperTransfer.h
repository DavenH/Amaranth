#pragma once

#include <Array/Buffer.h>
#include <Array/ScopedAlloc.h>
#include <Curve/Rasterization/Rasterizer/RasterizerViews.h>

class WaveshaperTransfer {
public:
    enum { tableResolution = 2048 };

    WaveshaperTransfer();

    void clearTable();
    void rasterizeFrom(const Rasterization::SamplerView& sampler, float padding);
    void applyLookup(Buffer<float> buffer) const;
    void process(Buffer<float> buffer, float preGain, float postGain) const;

    float lookup(float value) const;

private:
    ScopedAlloc<float> table;
};

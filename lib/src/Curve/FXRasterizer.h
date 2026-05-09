#pragma once

#include <App/SingletonAccessor.h>

#include "MeshRasterizer.h"
#include "Rasterization/Adapters/FxRasterizerAdapter.h"
#include "Rasterization/Interfaces/WaveformProvider.h"

class FXRasterizer:
        public MeshRasterizer
    ,   public SingletonAccessor
    ,   public Rasterization::WaveformProvider {
    JUCE_LEAK_DETECTOR(FXRasterizer)

public:
    explicit FXRasterizer(SingletonRepo* repo, const String& name = String());
    void setVertices(vector<Vertex*>* vertices);

    bool canRasterizeWaveform() const override;
    bool isBipolar() const override;
    void updateWaveform(UpdateType updateType) override;
    double sampleWithInterval(Buffer<float> buffer, double delta, double phase) override;
    float samplePerfectly(double delta, Buffer<float> buffer, double phase) override;

private:
    Rasterization::RasterizationRequest createFxRequest();

    Rasterization::FxRasterizerAdapter adapter;
};

#pragma once

#include <Curve/Rasterization/Rasterizer/VoiceRasterizer.h>
#include <Obj/MorphPosition.h>

#include "SourceRenderPerformance.h"

class Mesh;

namespace CycleDsp {

struct ChainedRasterizationRequest {
    Mesh* mesh {};
    Rasterization::VoiceCycleState* state {};
    MorphPosition morph { 0.5f, 0.5f, 0.5f };
    float phaseCycles {};
    double angleDelta {};
    int noiseSeed {};
};

struct FixedFrameRasterizationRequest {
    Mesh* mesh {};
    MorphPosition morph { 0.5f, 0.5f, 0.5f };
    float phaseCycles {};
    int noiseSeed {};
};

class OscillatorLaneRasterizer {
public:
    static void prime(
            Rasterization::VoiceRasterizer& rasterizer,
            const ChainedRasterizationRequest& request,
            SourceRenderPerformance* performance = nullptr);
    static void render(
            Rasterization::VoiceRasterizer& rasterizer,
            const ChainedRasterizationRequest& request,
            Buffer<float> output,
            SourceRenderPerformance* performance = nullptr);
    static bool renderFixedFrame(
            Rasterization::VoiceRasterizer& rasterizer,
            const FixedFrameRasterizationRequest& request,
            Buffer<float> output,
            SourceRenderPerformance* performance = nullptr);
};

}

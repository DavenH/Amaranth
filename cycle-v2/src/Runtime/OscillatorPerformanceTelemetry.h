#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <Audio/CycleDsp/SourceRenderPerformance.h>

namespace CycleV2 {

enum class OscillatorRecipeStage : uint8_t {
    TimeSourceRendering,
    SpectralSourceRendering,
    ForwardTransform,
    InverseTransform,
    GraphCombining,
    Count
};

static constexpr size_t oscillatorRecipeStageCount
        = static_cast<size_t>(OscillatorRecipeStage::Count);

struct OscillatorRegionPerformanceCounts {
    uint64_t regionDurationMicroseconds {};
    uint64_t recipeDurationMicroseconds {};
    uint64_t laneDurationMicroseconds {};
    uint64_t mixDurationMicroseconds {};
    uint32_t regionRenderCount {};
    uint32_t recipeRenderCount {};
    uint32_t laneCycleCount {};
    uint32_t mixedLaneCount {};
    std::array<uint64_t, oscillatorRecipeStageCount> recipeStageDurations {};
    std::array<uint32_t, oscillatorRecipeStageCount> recipeStageOperationCounts {};
    CycleDsp::SourceRenderPerformance timeSources;
    CycleDsp::SourceRenderPerformance spectralSources;
};

}

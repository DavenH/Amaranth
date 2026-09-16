#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <Curve/Rasterization/WaveformBakeWork.h>

namespace CycleDsp {

enum class SourceRenderStage : uint8_t {
    MorphResolution,
    Rasterization,
    Sampling,
    Gain,
    StereoCopy,
    Count
};

struct SourceRenderPerformance {
    static constexpr size_t stageCount = static_cast<size_t>(SourceRenderStage::Count);

    std::array<uint64_t, stageCount> nanoseconds {};
    std::array<uint64_t, stageCount> operations {};
    Rasterization::WaveformBakeWork waveform;

    void add(const SourceRenderPerformance& other) noexcept;
};

class ScopedSourceRenderStage {
public:
    ScopedSourceRenderStage(SourceRenderPerformance* counts, SourceRenderStage stage) noexcept;
    ~ScopedSourceRenderStage();

    ScopedSourceRenderStage(const ScopedSourceRenderStage&) = delete;
    ScopedSourceRenderStage& operator=(const ScopedSourceRenderStage&) = delete;

    void finish() noexcept;

private:
    SourceRenderPerformance* counts;
    size_t stageIndex;
    uint64_t startNanoseconds;
};

}

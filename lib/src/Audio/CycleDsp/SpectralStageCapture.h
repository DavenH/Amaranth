#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <JuceHeader.h>

#include <Array/Buffer.h>
#include <Array/ScopedAlloc.h>

namespace CycleDsp {

enum class SpectralStage {
    TimeFrame,
    ForwardFft,
    PostLayerSpectrum,
    ReconstructedFrame
};

struct SpectralStageFrame {
    SpectralStage stage { SpectralStage::TimeFrame };
    size_t frameIndex {};
    uint64_t frontier {};
    int midiNote {};
    int channel {};
    Buffer<float> primary;
    Buffer<float> secondary;
};

class SpectralStageCaptureSink {
public:
    virtual ~SpectralStageCaptureSink() = default;
    virtual void capture(const SpectralStageFrame& frame) noexcept = 0;
};

struct CapturedSpectralStage {
    SpectralStage stage { SpectralStage::TimeFrame };
    size_t frameIndex {};
    uint64_t frontier {};
    int midiNote {};
    int channel {};
    Buffer<float> primary;
    Buffer<float> secondary;
    bool captured {};
};

class SpectralStageCaptureRecorder final : public SpectralStageCaptureSink {
public:
    static constexpr int stageCount = 4;
    static constexpr int channelCount = 2;

    bool prepare(int maximumValueCount, size_t targetFrameIndex);
    void reset();
    void capture(const SpectralStageFrame& frame) noexcept override;

    const CapturedSpectralStage* record(
            SpectralStage stage,
            int channel) const;
    bool write(const juce::File& manifest, juce::String& error) const;

private:
    static int stageIndex(SpectralStage stage);

    int maximumValues {};
    size_t targetFrame {};
    ScopedAlloc<float> payloadMemory;
    std::array<CapturedSpectralStage, stageCount * channelCount> records;
};

juce::String spectralStageName(SpectralStage stage);

}

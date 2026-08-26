#pragma once

#include <Audio/CycleDsp/CycleDelay.h>

#include "Runtime/NodeDspConfiguration.h"
#include "Runtime/UnarySignalProcessor.h"

namespace CycleV2 {

struct DelayConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::Delay; }
    bool isEnabled() const override { return enabled; }

    bool enabled { true };
    float time { 0.5f };
    float feedback { 0.5f };
    float spin { 1.f };
    float wet { 0.9f };
    float spinIterations {};
};

class DelaySignalProcessor : public IUnarySignalOperation {
public:
    void configure(const DelayConfiguration& configuration, const AudioProcessTiming& timing);
    void beginBlock(size_t frameCount) override;
    void beginTraversalGrid(size_t columns, size_t rows) override;
    void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) override;

private:
    CycleDsp::CycleDelay blockDelays[2];
    CycleDsp::CycleDelay traversalDelays[2];
    bool processingTraversal {};
    double bpm { 120.0 };
    int beatsPerMeasure { 4 };
};

}

#pragma once

#include "../../Runtime/UnarySignalProcessor.h"
#include "../../Runtime/NodeDspConfiguration.h"

#include <Algo/Oversampler.h>
#include <Audio/WaveshaperTransfer.h>

namespace CycleV2 {

struct WaveshaperConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::Waveshaper; }
    bool isEnabled() const override { return enabled; }

    bool enabled { true };
    std::shared_ptr<const WaveshaperTransfer> transfer;
    float preGain { 1.f };
    float postGain { 1.f };
    int oversampleFactor { 1 };
};

class WaveshaperSignalProcessor :
        public IUnarySignalOperation {
public:
    static std::shared_ptr<const WaveshaperConfiguration> buildConfiguration(
            const std::vector<NodeParameter>& parameters,
            const NodeModelStatePtr& model = {});

    void prepareExecution(const AudioExecutionSpec& spec);
    void adoptConfiguration(const PublishedNodeConfiguration& published);

    void beginBlock(size_t frameCount) override;
    void beginTraversalGrid(size_t columns, size_t rows) override;
    void endTraversalGrid() override;
    void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) override;

private:
    Oversampler oversampler { 16 };
    ScopedAlloc<float> oversampleMemory;
    float preGain { 1.f };
    float postGain { 1.f };
    int oversampleFactor { 1 };
    bool useOversampling {};
    uint64_t adoptedRevision {};
    std::shared_ptr<const WaveshaperConfiguration> configuration;
};

}

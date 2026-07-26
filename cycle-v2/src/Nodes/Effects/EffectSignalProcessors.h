#pragma once

#include "../../Runtime/UnarySignalProcessor.h"
#include "../../Runtime/NodeDspConfiguration.h"
#include "PreparedConvolverPair.h"

#include <Algo/ConvReverb.h>
#include <Algo/Oversampler.h>
#include <Audio/CycleDsp/CycleDelay.h>
#include <Audio/CycleDsp/EffectParameterMapping.h>
#include <Audio/CycleDsp/EqualizerCore.h>
#include <Audio/CycleDsp/IrModel.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>

#include <array>

namespace CycleV2 {

struct IrConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::ImpulseResponse; }
    bool isEnabled() const override { return enabled; }

    bool enabled { true };
    std::vector<float> impulse;
    float postGain { 1.f };
};

struct ReverbConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::Reverb; }
    bool isEnabled() const override { return enabled; }

    bool enabled { true };
    std::array<std::vector<float>, 2> kernels;
    float width { 1.f };
    float wetLevel { 0.1f };
};

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

struct EqualizerConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::Equalizer; }
    bool isEnabled() const override { return enabled; }

    bool enabled { true };
    std::array<float, CycleDsp::equalizerBandCount> frequencies {};
    std::array<float, CycleDsp::equalizerBandCount> gains {};
};

class IrSignalProcessor :
        public IUnarySignalOperation {
public:
    static std::shared_ptr<const IrConfiguration> buildConfiguration(
            const std::vector<NodeParameter>& parameters,
            const NodeModelStatePtr& model = {});

    void prepareExecution(const AudioExecutionSpec& spec);
    void adoptConfiguration(const PublishedNodeConfiguration& published);

    void beginBlock(size_t frameCount) override;
    void beginTraversalGrid(size_t columns, size_t rows) override;
    void endTraversalGrid() override;
    void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) override;

private:
    void prepareBlockConvolver(size_t blockSize);
    void prepareTraversalConvolver(size_t rowCount);
    void prepareConvolver(BlockConvolver& convolver, size_t frameCount);

    PreparedConvolverPair<BlockConvolver> convolvers;
    float postGain { 1.f };
    uint64_t adoptedRevision {};
    std::shared_ptr<const IrConfiguration> configuration;
};

class DelaySignalProcessor :
        public IUnarySignalOperation {
public:
    void configure(
            const std::vector<NodeParameter>& parameters,
            const AudioProcessTiming& timing);
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

class ReverbSignalProcessor :
        public IUnarySignalOperation {
public:
    static std::shared_ptr<const ReverbConfiguration> buildConfiguration(
            const std::vector<NodeParameter>& parameters);

    void prepareExecution(const AudioExecutionSpec& spec);
    void adoptConfiguration(const PublishedNodeConfiguration& published);

    void beginChannelBlock(size_t channelCount, size_t frameCount) override;
    void beginChannelTraversalGrid(size_t channelCount, size_t columns, size_t rows) override;
    void beginBlock(size_t frameCount) override;
    void beginTraversalGrid(size_t columns, size_t rows) override;
    void endTraversalGrid() override;
    void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) override;

private:
    void prepareBlockConvolver(size_t blockSize);
    void prepareTraversalConvolver(size_t rowCount);
    void prepareConvolver(size_t channel, ConvReverb& convolver, size_t frameCount);
    void mixPendingBuffers(size_t frameCount);

    std::array<std::vector<float>, 2> dryBuffers;
    std::array<std::vector<float>, 2> mixBuffers;
    std::array<PreparedConvolverPair<ConvReverb>, 2> convolvers;
    std::array<Buffer<float>, 2> pendingWet;
    size_t activeChannelCount { 1 };
    float wetLevel { 0.1f };
    uint64_t adoptedRevision {};
    std::shared_ptr<const ReverbConfiguration> configuration;
};

class EqualizerSignalProcessor :
        public IUnarySignalOperation {
public:
    static std::shared_ptr<const EqualizerConfiguration> buildConfiguration(
            const std::vector<NodeParameter>& parameters);

    EqualizerSignalProcessor();

    void prepareExecution(const AudioExecutionSpec& spec);
    void adoptConfiguration(const PublishedNodeConfiguration& published);
    void beginBlock(size_t frameCount) override;
    void beginTraversalGrid(size_t columns, size_t rows) override;
    void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) override;

private:
    void configure(CycleDsp::EqualizerCore& core);

    bool processingTraversal {};
    double sampleRate { 44100.0 };
    uint64_t adoptedRevision {};
    CycleDsp::EqualizerCore blockCore;
    CycleDsp::EqualizerCore traversalCore;
    std::shared_ptr<const EqualizerConfiguration> configuration;
};

}

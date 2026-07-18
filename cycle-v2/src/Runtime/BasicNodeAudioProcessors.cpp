#include <Array/Buffer.h>

#include <algorithm>

#include "AudioProcessContextUtils.h"
#include "AudioProcessorFactories.h"
#include "BinarySignalProcessor.h"
#include "NodeAudioProcessorSupport.h"

namespace CycleV2 {

namespace {

constexpr size_t kDefaultTraversalColumns = 8;

void publishSourceTraversalGrid(
        SignalPayload& payload,
        size_t columns,
        float level,
        bool image,
        const AudioProcessWorkArena* arena) {
    if (payload.block.samples.empty()) {
        payload.traversalGrid = {};
        return;
    }

    const size_t rows = payload.block.samples.size();
    configureTraversalGrid(
            payload.traversalGrid,
            columns,
            rows,
            makeTraversalGridMetadata(
                    payload.domain,
                    columns,
                    rows,
                    image ? TraversalGridAxis::ImageX : TraversalGridAxis::Phase,
                    image ? TraversalGridAxis::ImageY : TraversalGridAxis::Time),
            arena);

    const float rowDenominator = rows > 1 ? (float) (rows - 1) : 1.f;
    const float columnDenominator = columns > (image ? 1u : 0u)
            ? (float) (columns - (image ? 1u : 0u))
            : 1.f;

    for (size_t column = 0; column < columns; ++column) {
        const float columnPosition = (float) column / columnDenominator;

        for (size_t row = 0; row < rows; ++row) {
            const float rowPosition = (float) row / rowDenominator;
            if (image) {
                payload.traversalGrid.values[column * rows + row]
                        = level * (0.65f * columnPosition + 0.35f * rowPosition);
                continue;
            }

            float value = rowPosition + columnPosition;
            if (value >= 1.f) {
                value -= 1.f;
            }

            payload.traversalGrid.values[column * rows + row] = value * level;
        }
    }
}

class SourceAudioProcessor final : public NodeAudioProcessor {
public:
    explicit SourceAudioProcessor(bool image) : image(image) {}

    AudioModuleRole role() const override {
        return image ? AudioModuleRole::ImageSource : AudioModuleRole::WaveSource;
    }

    void adoptConfiguration(const PublishedNodeConfiguration& published) override {
        configuration = std::dynamic_pointer_cast<const SourceNodeConfiguration>(published.value);
    }

    void process(AudioProcessContext& context) override {
        auto output = makeOutputPayload(context, 0);
        if (context.frameCount == 0) {
            publishSingleOutput(context, std::move(output));
            return;
        }

        const float denominator = context.frameCount > 1
                ? (float) (context.frameCount - 1)
                : 1.f;
        const float level = configuration != nullptr
                ? configuration->level
                : parameterFloat(processParameters(context), "level", 1.f);
        payloadBuffer(output, context.frameCount).ramp(0.f, level / denominator);

        if (context.captureTraversalGrid) {
            publishSourceTraversalGrid(
                    output,
                    image ? std::max(kDefaultTraversalColumns, context.frameCount)
                          : kDefaultTraversalColumns,
                    level,
                    image,
                    context.workArena);
        }

        publishSingleOutput(context, std::move(output));
    }

private:
    bool image {};
    std::shared_ptr<const SourceNodeConfiguration> configuration;
};

class BinaryAudioProcessor final : public NodeAudioProcessor {
public:
    BinaryAudioProcessor(AudioModuleRole role, BinarySignalOperation operation) :
            processorRole(role), operation(operation) {}

    AudioModuleRole role() const override { return processorRole; }

    void process(AudioProcessContext& context) override {
        auto output = makeOutputPayload(context, 0);
        SignalPayload* left = inputAt(context, 0);
        SignalPayload* right = inputAt(context, 1);

        if (operation == BinarySignalOperation::Multiply
                && (left == nullptr || right == nullptr)) {
            clearOutput(context);
            return;
        }

        processor.process(output, left, right, operation, context.frameCount, context.workArena);
        publishSingleOutput(context, std::move(output));
    }

private:
    AudioModuleRole processorRole {};
    BinarySignalOperation operation { BinarySignalOperation::Add };
    BinarySignalProcessor processor;
};

class PassthroughAudioProcessor final : public NodeAudioProcessor {
public:
    explicit PassthroughAudioProcessor(AudioModuleRole role) : processorRole(role) {}

    AudioModuleRole role() const override { return processorRole; }
    void process(AudioProcessContext& context) override { processPassthrough(context); }

private:
    AudioModuleRole processorRole {};
};

class SilentAudioProcessor final : public NodeAudioProcessor {
public:
    explicit SilentAudioProcessor(AudioModuleRole role) : processorRole(role) {}

    AudioModuleRole role() const override { return processorRole; }
    void process(AudioProcessContext& context) override { clearOutput(context); }

private:
    AudioModuleRole processorRole {};
};

class StereoSplitAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return AudioModuleRole::StereoSplit; }

    void process(AudioProcessContext& context) override {
        SignalPayload* input = inputAt(context, 0);
        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        auto left = makeOutputPayload(context, 0);
        auto right = makeOutputPayload(context, 1);
        copyPayloadBlockExpandingScalars(left, *input, context.frameCount);
        copyPayloadBlockExpandingScalars(right, *input, context.frameCount);
        copyTraversalGrid(left, input->traversalGrid);
        copyTraversalGrid(right, input->traversalGrid);
        publishOutputs(context, std::move(left), std::move(right));
    }
};

}

std::unique_ptr<NodeAudioProcessor> createWaveSourceAudioProcessor() {
    return std::make_unique<SourceAudioProcessor>(false);
}

std::unique_ptr<NodeAudioProcessor> createImageSourceAudioProcessor() {
    return std::make_unique<SourceAudioProcessor>(true);
}

std::unique_ptr<NodeAudioProcessor> createAddAudioProcessor() {
    return std::make_unique<BinaryAudioProcessor>(
            AudioModuleRole::Add,
            BinarySignalOperation::Add);
}

std::unique_ptr<NodeAudioProcessor> createMultiplyAudioProcessor() {
    return std::make_unique<BinaryAudioProcessor>(
            AudioModuleRole::Multiply,
            BinarySignalOperation::Multiply);
}

std::unique_ptr<NodeAudioProcessor> createStereoJoinAudioProcessor() {
    return std::make_unique<BinaryAudioProcessor>(
            AudioModuleRole::StereoJoin,
            BinarySignalOperation::Add);
}

std::unique_ptr<NodeAudioProcessor> createStereoSplitAudioProcessor() {
    return std::make_unique<StereoSplitAudioProcessor>();
}

std::unique_ptr<NodeAudioProcessor> createOutputAudioProcessor() {
    return std::make_unique<PassthroughAudioProcessor>(AudioModuleRole::Output);
}

std::unique_ptr<NodeAudioProcessor> createGenericAudioProcessor() {
    return std::make_unique<PassthroughAudioProcessor>(AudioModuleRole::GenericProcessor);
}

std::unique_ptr<NodeAudioProcessor> createVoiceContextAudioProcessor() {
    return std::make_unique<SilentAudioProcessor>(AudioModuleRole::VoiceContext);
}

std::unique_ptr<NodeAudioProcessor> createGuideCurveAudioProcessor() {
    return std::make_unique<SilentAudioProcessor>(AudioModuleRole::GuideCurve);
}

}

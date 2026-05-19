#include <Array/Buffer.h>

#include "NodeAudioProcessor.h"

namespace CycleV2 {

namespace {

Buffer<float> outputBuffer(AudioProcessContext& context) {
    return { context.output.samples.data(), (int) context.frameCount };
}

Buffer<float> blockBuffer(AudioProcessBlock& block, size_t frameCount) {
    return { block.samples.data(), (int) frameCount };
}

void ensureOutput(AudioProcessContext& context) {
    context.output.samples.resize(context.frameCount);

    if (!context.outputPorts.empty()) {
        context.output.domain = context.outputPorts.front().domain;
        context.output.channelLayout = context.outputPorts.front().channelLayout;
    }
}

void clearOutput(AudioProcessContext& context) {
    ensureOutput(context);
    outputBuffer(context).zero();
    context.outputs = { context.output };
}

AudioProcessBlock makeOutputBlock(const AudioOutputPort& port, size_t frameCount) {
    AudioProcessBlock block;
    block.samples.resize(frameCount);
    block.domain = port.domain;
    block.channelLayout = port.channelLayout;
    return block;
}

AudioProcessBlock makeOutputBlock(AudioProcessContext& context, size_t index) {
    if (index < context.outputPorts.size()) {
        return makeOutputBlock(context.outputPorts[index], context.frameCount);
    }

    AudioProcessBlock block;
    block.samples.resize(context.frameCount);
    return block;
}

Buffer<float> outputVector(AudioProcessBlock& block, size_t frameCount) {
    return { block.samples.data(), (int) frameCount };
}

void publishSingleOutput(AudioProcessContext& context) {
    context.outputs = { context.output };
}

void publishOutputs(AudioProcessContext& context, std::vector<AudioProcessBlock> outputs) {
    context.outputs = std::move(outputs);

    if (!context.outputs.empty()) {
        context.output = context.outputs.front();
    } else {
        clearOutput(context);
    }
}

AudioProcessBlock* inputAt(AudioProcessContext& context, size_t index) {
    if (index >= context.inputs.size()) {
        return nullptr;
    }

    if (context.inputs[index].samples.size() < context.frameCount) {
        return nullptr;
    }

    return &context.inputs[index];
}

float parameterFloat(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        float fallback) {
    for (const auto& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value.getFloatValue();
        }
    }

    return fallback;
}

class FixedRoleProcessor final : public NodeAudioProcessor {
public:
    explicit FixedRoleProcessor(AudioModuleRole roleToUse) : processorRole(roleToUse) {}

    AudioModuleRole role() const override { return processorRole; }

    void process(AudioProcessContext& context) override {
        switch (processorRole) {
            case AudioModuleRole::WaveSource:
            case AudioModuleRole::ImageSource:
                processRampSource(context);
                break;

            case AudioModuleRole::Envelope:
                processEnvelope(context);
                break;

            case AudioModuleRole::Fft:
                processFft(context);
                break;

            case AudioModuleRole::Ifft:
                processIfft(context);
                break;

            case AudioModuleRole::StereoSplit:
                processStereoSplit(context);
                break;

            case AudioModuleRole::StereoJoin:
                processStereoJoin(context);
                break;

            case AudioModuleRole::Add:
                processAdd(context);
                break;

            case AudioModuleRole::Multiply:
                processMultiply(context);
                break;

            case AudioModuleRole::Output:
            case AudioModuleRole::MeshSource:
            case AudioModuleRole::ImpulseResponse:
            case AudioModuleRole::Waveshaper:
            case AudioModuleRole::Reverb:
            case AudioModuleRole::Delay:
            case AudioModuleRole::GenericProcessor:
                processPassthrough(context);
                break;

            case AudioModuleRole::VoiceContext:
            case AudioModuleRole::GuideCurve:
            case AudioModuleRole::None:
            default:
                clearOutput(context);
                break;
        }
    }

private:
    void processRampSource(AudioProcessContext& context) const {
        ensureOutput(context);

        if (context.frameCount == 0) {
            return;
        }

        const float denominator = context.frameCount > 1 ? (float) (context.frameCount - 1) : 1.f;
        const float level = parameterFloat(context.parameters, "level", 1.f);

        outputBuffer(context).ramp(0.f, level / denominator);
        publishSingleOutput(context);
    }

    void processEnvelope(AudioProcessContext& context) const {
        ensureOutput(context);
        const float level = parameterFloat(context.parameters, "level", 1.f);

        outputBuffer(context).set(level);
        publishSingleOutput(context);
    }

    void processFft(AudioProcessContext& context) const {
        AudioProcessBlock* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        AudioProcessBlock magnitude = makeOutputBlock(context, 0);
        AudioProcessBlock phase = makeOutputBlock(context, 1);

        blockBuffer(*input, context.frameCount).copyTo(outputVector(magnitude, context.frameCount));
        outputVector(phase, context.frameCount).zero();

        publishOutputs(context, { std::move(magnitude), std::move(phase) });
    }

    void processIfft(AudioProcessContext& context) const {
        AudioProcessBlock* magnitude = inputAt(context, 0);

        if (magnitude == nullptr) {
            clearOutput(context);
            return;
        }

        ensureOutput(context);
        blockBuffer(*magnitude, context.frameCount).copyTo(outputBuffer(context));
        publishSingleOutput(context);
    }

    void processStereoSplit(AudioProcessContext& context) const {
        AudioProcessBlock* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        AudioProcessBlock left = makeOutputBlock(context, 0);
        AudioProcessBlock right = makeOutputBlock(context, 1);
        blockBuffer(*input, context.frameCount).copyTo(outputVector(left, context.frameCount));
        blockBuffer(*input, context.frameCount).copyTo(outputVector(right, context.frameCount));

        publishOutputs(context, { std::move(left), std::move(right) });
    }

    void processStereoJoin(AudioProcessContext& context) const {
        processAdd(context);
    }

    void processAdd(AudioProcessContext& context) const {
        ensureOutput(context);
        AudioProcessBlock* left = inputAt(context, 0);
        AudioProcessBlock* right = inputAt(context, 1);
        Buffer<float> output = outputBuffer(context);

        output.zero();

        if (left != nullptr) {
            blockBuffer(*left, context.frameCount).copyTo(output);
        }

        if (right != nullptr) {
            output.add(blockBuffer(*right, context.frameCount));
        }

        publishSingleOutput(context);
    }

    void processMultiply(AudioProcessContext& context) const {
        ensureOutput(context);
        AudioProcessBlock* left = inputAt(context, 0);
        AudioProcessBlock* right = inputAt(context, 1);

        if (left == nullptr || right == nullptr) {
            clearOutput(context);
            return;
        }

        Buffer<float> output = outputBuffer(context);
        blockBuffer(*left, context.frameCount).copyTo(output);
        output.mul(blockBuffer(*right, context.frameCount));
        publishSingleOutput(context);
    }

    void processPassthrough(AudioProcessContext& context) const {
        ensureOutput(context);
        AudioProcessBlock* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        blockBuffer(*input, context.frameCount).copyTo(outputBuffer(context));
        publishSingleOutput(context);
    }

    AudioModuleRole processorRole {};
};

}

void NodeAudioProcessor::prepare(size_t) {}

std::unique_ptr<NodeAudioProcessor> NodeAudioProcessorFactory::create(AudioModuleRole role) const {
    if (role == AudioModuleRole::None) {
        return {};
    }

    return std::make_unique<FixedRoleProcessor>(role);
}

}

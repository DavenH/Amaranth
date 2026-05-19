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
}

void clearOutput(AudioProcessContext& context) {
    ensureOutput(context);
    outputBuffer(context).zero();
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
            case AudioModuleRole::StereoSplit:
            case AudioModuleRole::StereoJoin:
            case AudioModuleRole::Fft:
            case AudioModuleRole::Ifft:
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
    }

    void processEnvelope(AudioProcessContext& context) const {
        ensureOutput(context);
        const float level = parameterFloat(context.parameters, "level", 1.f);

        outputBuffer(context).set(level);
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
    }

    void processPassthrough(AudioProcessContext& context) const {
        ensureOutput(context);
        AudioProcessBlock* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        blockBuffer(*input, context.frameCount).copyTo(outputBuffer(context));
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

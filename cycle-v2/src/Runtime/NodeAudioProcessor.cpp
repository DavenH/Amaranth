#include "NodeAudioProcessor.h"

namespace CycleV2 {

namespace {

void ensureOutput(AudioProcessContext& context) {
    context.output.samples.resize(context.frameCount);
}

void clearOutput(AudioProcessContext& context) {
    ensureOutput(context);

    for (auto& sample : context.output.samples) {
        sample = 0.f;
    }
}

const AudioProcessBlock* inputAt(const AudioProcessContext& context, size_t index) {
    if (index >= context.inputs.size()) {
        return nullptr;
    }

    if (context.inputs[index].samples.size() < context.frameCount) {
        return nullptr;
    }

    return &context.inputs[index];
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

        for (size_t i = 0; i < context.frameCount; ++i) {
            context.output.samples[i] = (float) i / denominator;
        }
    }

    void processEnvelope(AudioProcessContext& context) const {
        ensureOutput(context);

        for (auto& sample : context.output.samples) {
            sample = 1.f;
        }
    }

    void processAdd(AudioProcessContext& context) const {
        ensureOutput(context);
        const AudioProcessBlock* left = inputAt(context, 0);
        const AudioProcessBlock* right = inputAt(context, 1);

        for (size_t i = 0; i < context.frameCount; ++i) {
            const float a = left == nullptr ? 0.f : left->samples[i];
            const float b = right == nullptr ? 0.f : right->samples[i];
            context.output.samples[i] = a + b;
        }
    }

    void processMultiply(AudioProcessContext& context) const {
        ensureOutput(context);
        const AudioProcessBlock* left = inputAt(context, 0);
        const AudioProcessBlock* right = inputAt(context, 1);

        for (size_t i = 0; i < context.frameCount; ++i) {
            const float a = left == nullptr ? 0.f : left->samples[i];
            const float b = right == nullptr ? 0.f : right->samples[i];
            context.output.samples[i] = a * b;
        }
    }

    void processPassthrough(AudioProcessContext& context) const {
        ensureOutput(context);
        const AudioProcessBlock* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        for (size_t i = 0; i < context.frameCount; ++i) {
            context.output.samples[i] = input->samples[i];
        }
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

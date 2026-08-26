#include <Array/Buffer.h>

#include <algorithm>

#include "Nodes/Control/ModulationSource.h"

#include "Graph/NodeParameterMap.h"

#include "Runtime/AudioProcessContextUtils.h"

namespace CycleV2 {

namespace {

constexpr int kModWheelController = 1;

float controllerValue(const PreviewControlContext& context, int controller) {
    return context.controllers[(size_t) jlimit(0, 127, controller)];
}

PreviewControlContext previewContextForVoice(const AudioVoiceContext& voice) {
    PreviewControlContext context;
    context.voiceTime = voice.controls.normalizedVoiceTime;
    context.velocity = voice.controls.velocity;
    context.noteNumber = voice.controls.noteNumber;
    context.lowestNote = voice.controls.lowestNote;
    context.highestNote = voice.controls.highestNote;
    context.channelPressure = voice.controls.channelPressure;
    context.controllers = voice.controls.controllers;
    return context;
}

class ModulationSourceAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return AudioModuleRole::ModulationSource; }

    void adoptConfiguration(const PublishedNodeConfiguration& published) override {
        configuration = std::dynamic_pointer_cast<const ModulationSourceConfiguration>(
                published.value);
    }

    void process(AudioProcessContext& context) override {
        auto output = makeOutputPayload(context, 0);
        Buffer<float> values = payloadBuffer(output, context.frameCount);
        if (configuration == nullptr || context.frameCount == 0) {
            values.zero();
            publishSingleOutput(context, std::move(output));
            return;
        }

        const AudioVoiceContext& voice = processVoice(context);
        ModulationSource::renderAudioBlock(*configuration, voice, values);
        publishSingleOutput(context, std::move(output));
    }

private:
    std::shared_ptr<const ModulationSourceConfiguration> configuration;
};

class ModulationSourcePreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::ModulationSource; }

    void render(PreviewProcessContext& context) override {
        const auto configuration = ModulationSource::buildConfiguration(context.parameters);
        const PreviewControlContext controls = context.controlContext != nullptr
                ? *context.controlContext
                : PreviewControlContext {};
        context.primary.resize(context.pointCount);
        context.secondary.clear();
        context.domain = PortDomain::ControlSignal;

        if (configuration->mode == ModulationSourceMode::VoiceTime
                && controls.traverseVoiceTime) {
            const float denominator = context.pointCount > 1
                    ? (float) (context.pointCount - 1)
                    : 1.f;
            for (size_t column = 0; column < context.pointCount; ++column) {
                context.primary[column] = (float) column / denominator;
            }
            context.gridColumns = context.pointCount;
            context.gridRows = 1;
            return;
        }

        std::fill(
                context.primary.begin(),
                context.primary.end(),
                ModulationSource::evaluate(*configuration, controls));
    }
};

}

ModulationSourceMode ModulationSource::modeFromId(const String& id) {
    if (id == "voiceTime") {
        return ModulationSourceMode::VoiceTime;
    }
    if (id == "velocity") {
        return ModulationSourceMode::Velocity;
    }
    if (id == "inverseVelocity") {
        return ModulationSourceMode::InverseVelocity;
    }
    if (id == "keyScale") {
        return ModulationSourceMode::KeyScale;
    }
    if (id == "channelPressure") {
        return ModulationSourceMode::ChannelPressure;
    }
    if (id == "midiCC") {
        return ModulationSourceMode::MidiController;
    }
    if (id == "constant") {
        return ModulationSourceMode::Constant;
    }
    return ModulationSourceMode::ModWheel;
}

String ModulationSource::idForMode(ModulationSourceMode mode) {
    switch (mode) {
        case ModulationSourceMode::VoiceTime:
            return "voiceTime";
        case ModulationSourceMode::Velocity:
            return "velocity";
        case ModulationSourceMode::InverseVelocity:
            return "inverseVelocity";
        case ModulationSourceMode::KeyScale:
            return "keyScale";
        case ModulationSourceMode::ModWheel:
            return "modWheel";
        case ModulationSourceMode::ChannelPressure:
            return "channelPressure";
        case ModulationSourceMode::MidiController:
            return "midiCC";
        case ModulationSourceMode::Constant:
            return "constant";
    }
    return "modWheel";
}

float ModulationSource::normalizeKey(int note, int lowestNote, int highestNote) {
    if (highestNote <= lowestNote) {
        return 0.f;
    }
    return jlimit(0.f, 1.f,
            (float) (note - lowestNote) / (float) (highestNote - lowestNote));
}

float ModulationSource::evaluate(
        const ModulationSourceConfiguration& configuration,
        const PreviewControlContext& context) {
    switch (configuration.mode) {
        case ModulationSourceMode::VoiceTime:
            return jlimit(0.f, 1.f, context.voiceTime);
        case ModulationSourceMode::Velocity:
            return jlimit(0.f, 1.f, context.velocity);
        case ModulationSourceMode::InverseVelocity:
            return 1.f - jlimit(0.f, 1.f, context.velocity);
        case ModulationSourceMode::KeyScale:
            return normalizeKey(context.noteNumber, context.lowestNote, context.highestNote);
        case ModulationSourceMode::ModWheel:
            return controllerValue(context, kModWheelController);
        case ModulationSourceMode::ChannelPressure:
            return jlimit(0.f, 1.f, context.channelPressure);
        case ModulationSourceMode::MidiController:
            return controllerValue(context, configuration.controller);
        case ModulationSourceMode::Constant:
            return jlimit(0.f, 1.f, configuration.constant);
    }
    return 0.f;
}

std::shared_ptr<const ModulationSourceConfiguration> ModulationSource::buildConfiguration(
        const std::vector<NodeParameter>& parameters) {
    return std::make_shared<ModulationSourceConfiguration>(
            buildConfiguration(parameters, {}, "modWheel"));
}

ModulationSourceConfiguration ModulationSource::buildConfiguration(
        const std::vector<NodeParameter>& parameters,
        const String& prefix,
        const String& defaultSource) {
    const String sourceId = prefix.isEmpty() ? "source" : prefix + "Source";
    const String controllerId = prefix.isEmpty() ? "controller" : prefix + "Controller";
    const String constantId = prefix.isEmpty() ? "constant" : prefix + "Constant";
    const NodeParameterMap parameterMap(parameters);
    ModulationSourceConfiguration configuration;
    configuration.mode = modeFromId(parameterMap.stringValue(sourceId, defaultSource));
    configuration.controller = jlimit(0, 127,
            parameterMap.intValue(controllerId, 1));
    configuration.constant = jlimit(0.f, 1.f,
            parameterMap.floatValue(constantId, 0.5f));
    return configuration;
}

void ModulationSource::renderAudioBlock(
        const ModulationSourceConfiguration& configuration,
        const AudioVoiceContext& voice,
        Buffer<float> values) {
    PreviewControlContext current = previewContextForVoice(voice);
    if (configuration.mode == ModulationSourceMode::VoiceTime) {
        values.ramp(
                jlimit(0.f, 1.f, voice.controls.normalizedVoiceTime),
                voice.controls.normalizedVoiceTimeIncrement);
        values.clip(0.f, 1.f);
        return;
    }

    const bool timedController = configuration.mode == ModulationSourceMode::ModWheel
            || configuration.mode == ModulationSourceMode::MidiController
            || configuration.mode == ModulationSourceMode::ChannelPressure;
    if (!timedController) {
        values.set(evaluate(configuration, current));
        return;
    }

    size_t offset = 0;
    for (const auto& event : voice.controlEvents) {
        const size_t eventOffset = jmin(event.sampleOffset, (size_t) values.size());
        if (eventOffset > offset) {
            values.section((int) offset, (int) (eventOffset - offset)).set(
                    evaluate(configuration, current));
        }

        if (event.kind == ControlEventKind::ChannelPressure) {
            current.channelPressure = event.value;
        } else {
            current.controllers[(size_t) jlimit(0, 127, event.controller)] = event.value;
        }
        offset = eventOffset;
    }

    if (offset < (size_t) values.size()) {
        values.section((int) offset, values.size() - (int) offset).set(
                evaluate(configuration, current));
    }
}

std::unique_ptr<NodeAudioProcessor> createModulationSourceAudioProcessor() {
    return std::make_unique<ModulationSourceAudioProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createModulationSourcePreviewProcessor() {
    return std::make_unique<ModulationSourcePreviewProcessor>();
}

}

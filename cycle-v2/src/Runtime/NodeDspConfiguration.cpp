#include "NodeDspConfiguration.h"

#include "../Nodes/Waveshaper/WaveshaperSignalProcessor.h"
#include "../Nodes/Effects/EffectSignalProcessors.h"
#include "../Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "../Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../Nodes/Trimesh/TrimeshMeshState.h"

#include <Curve/Mesh/Mesh.h>

namespace CycleV2 {

namespace {

std::shared_ptr<const TrimeshConfiguration> buildTrimeshConfiguration(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model) {
    auto configuration = std::make_shared<TrimeshConfiguration>();
    const NodeModelStatePtr modelToUse = model != nullptr
            ? model
            : TrimeshNodeModelCodec().createDefault();
    const auto typedModel = std::dynamic_pointer_cast<const TrimeshNodeModelState>(modelToUse);
    if (typedModel == nullptr) {
        return {};
    }
    configuration->mesh = typedModel->sharedMesh();
    configuration->morph = {
            typedParameterFloat(parameters, "yellow", 0.5f),
            typedParameterFloat(parameters, "red", 0.5f),
            typedParameterFloat(parameters, "blue", 0.5f)
    };
    const String axis = typedParameterString(parameters, "primaryAxis", "yellow");
    configuration->primaryViewAxis = axis == "red" ? Vertex::Red
            : (axis == "blue" ? Vertex::Blue : Vertex::Time);
    return configuration;
}

}

String NodeDspConfigurationFactory::keyFor(
        AudioModuleRole role,
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model,
        const AudioExecutionSpec& spec) const {
    String key((int) role);
    (void) spec;

    for (const auto& parameter : parameters) {
        key << ":" << parameter.id << "=" << parameter.value;
    }
    if (model != nullptr) {
        key << ":model=" << model->schemaId() << ":" << String((int64) model->revision());
    }

    return key;
}

std::shared_ptr<const INodeDspConfiguration> NodeDspConfigurationFactory::create(
        AudioModuleRole role,
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model,
        const AudioExecutionSpec&) const {
    using Factory = std::shared_ptr<const INodeDspConfiguration> (*)(
            AudioModuleRole,
            const std::vector<NodeParameter>&,
            const NodeModelStatePtr&);
    struct Registration {
        AudioModuleRole role;
        Factory factory;
    };
    static const Registration registrations[] {
        { AudioModuleRole::WaveSource, [](AudioModuleRole roleToUse, const auto& values, const auto&) {
            auto configuration = std::make_shared<SourceNodeConfiguration>();
            configuration->processorRole = roleToUse;
            configuration->level = typedParameterFloat(values, "level", 1.f);
            return std::shared_ptr<const INodeDspConfiguration>(configuration);
        } },
        { AudioModuleRole::ImageSource, [](AudioModuleRole roleToUse, const auto& values, const auto&) {
            auto configuration = std::make_shared<SourceNodeConfiguration>();
            configuration->processorRole = roleToUse;
            configuration->level = typedParameterFloat(values, "level", 1.f);
            return std::shared_ptr<const INodeDspConfiguration>(configuration);
        } },
        { AudioModuleRole::Fft, [](AudioModuleRole roleToUse, const auto& values, const auto&) {
            auto configuration = std::make_shared<FftNodeConfiguration>();
            configuration->processorRole = roleToUse;
            configuration->halfCycleCarry = false;
            return std::shared_ptr<const INodeDspConfiguration>(configuration);
        } },
        { AudioModuleRole::Ifft, [](AudioModuleRole roleToUse, const auto& values, const auto&) {
            auto configuration = std::make_shared<FftNodeConfiguration>();
            configuration->processorRole = roleToUse;
            configuration->halfCycleCarry = typedParameterString(values, "mode", "cyclic")
                    == "acyclicCarry";
            return std::shared_ptr<const INodeDspConfiguration>(configuration);
        } },
        { AudioModuleRole::MeshSource, [](AudioModuleRole, const auto& values, const auto& modelState) {
            return std::shared_ptr<const INodeDspConfiguration>(buildTrimeshConfiguration(values, modelState));
        } },
        { AudioModuleRole::Waveshaper, [](AudioModuleRole, const auto& values, const auto& modelState) {
            return std::shared_ptr<const INodeDspConfiguration>(
                    WaveshaperSignalProcessor::buildConfiguration(values, modelState));
        } },
        { AudioModuleRole::ImpulseResponse, [](AudioModuleRole, const auto& values, const auto& modelState) {
            return std::shared_ptr<const INodeDspConfiguration>(
                    IrSignalProcessor::buildConfiguration(values, modelState));
        } },
        { AudioModuleRole::Reverb, [](AudioModuleRole, const auto& values, const auto&) {
            return std::shared_ptr<const INodeDspConfiguration>(
                    ReverbSignalProcessor::buildConfiguration(values));
        } },
        { AudioModuleRole::Delay, [](AudioModuleRole, const auto& values, const auto&) {
            auto configuration = std::make_shared<DelayConfiguration>();
            configuration->enabled = typedParameterBool(values, "enabled", true);
            configuration->time = typedParameterFloat(values, "time", 0.5f);
            configuration->feedback = typedParameterFloat(values, "feedback", 0.5f);
            configuration->spin = typedParameterFloat(values, "spin", 1.f);
            configuration->wet = typedParameterFloat(values, "wet", 0.9f);
            configuration->spinIterations = typedParameterFloat(values, "spinIters", 0.f);
            return std::shared_ptr<const INodeDspConfiguration>(configuration);
        } },
        { AudioModuleRole::Equalizer, [](AudioModuleRole, const auto& values, const auto&) {
            return std::shared_ptr<const INodeDspConfiguration>(
                    EqualizerSignalProcessor::buildConfiguration(values));
        } },
        { AudioModuleRole::Envelope, [](AudioModuleRole, const auto& values, const auto& modelState) {
            return std::shared_ptr<const INodeDspConfiguration>(
                    EnvelopeSignalProcessor::buildConfiguration(values, modelState));
        } }
    };

    for (const auto& registration : registrations) {
        if (registration.role == role) {
            return registration.factory(role, parameters, model);
        }
    }

    return {};
}

}

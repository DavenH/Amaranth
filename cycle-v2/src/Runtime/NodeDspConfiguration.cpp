#include <Curve/Mesh/Mesh.h>

#include "Runtime/NodeDspConfiguration.h"

#include "Graph/NodeParameterMap.h"
#include "Nodes/Control/ModulationSource.h"
#include "Nodes/Control/ModulationTriple.h"
#include "Nodes/Delay/DelaySignalProcessor.h"
#include "Nodes/Effects/EffectSignalProcessors.h"
#include "Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Nodes/Trimesh/Dsp/TrimeshGuidePreparation.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Nodes/Unison/UnisonNode.h"
#include "Nodes/Waveshaper/WaveshaperSignalProcessor.h"

namespace CycleV2 {

namespace {

std::shared_ptr<TrimeshConfiguration> buildTrimeshConfiguration(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model,
        const NodeGraph* graph,
        const String& nodeId) {
    auto configuration = std::make_shared<TrimeshConfiguration>();
    const NodeModelStatePtr modelToUse = model != nullptr
            ? model
            : TrimeshNodeModelCodec().createDefault();
    const auto typedModel = std::dynamic_pointer_cast<const TrimeshNodeModelState>(modelToUse);
    if (typedModel == nullptr) {
        return {};
    }
    const NodeParameterMap parameterMap(parameters);
    configuration->mesh = typedModel->sharedMesh();
    if (graph != nullptr) {
        const Node* node = graph->findNode(nodeId);
        if (node != nullptr) {
            auto guides = TrimeshGuidePreparation::prepare(
                    *graph,
                    *node,
                    *configuration->mesh);
            configuration->mesh = std::move(guides.mesh);
            configuration->guideCurveProvider = std::move(guides.provider);
            configuration->guideAssignmentCount = guides.assignmentCount;
        }
    }
    configuration->morph = {
            parameterMap.floatValue("yellow", 0.5f),
            parameterMap.floatValue("red", 0.5f),
            parameterMap.floatValue("blue", 0.5f)
    };
    const String axis = parameterMap.stringValue("primaryAxis", "yellow");
    configuration->primaryViewAxis = axis == "red" ? Vertex::Red
            : (axis == "blue" ? Vertex::Blue : Vertex::Time);
    return configuration;
}

}

String NodeDspConfigurationFactory::keyFor(
        AudioModuleRole role,
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model,
        const AudioExecutionSpec& spec,
        const NodeGraph* graph,
        const String& nodeId) const {
    String key((int) role);
    (void) spec;

    for (const auto& parameter : parameters) {
        key << ":" << parameter.id << "=" << parameter.value;
    }
    if (model != nullptr) {
        key << ":model=" << model->schemaId() << ":" << String((int64) model->revision());
    }
    if (graph != nullptr) {
        key << TrimeshGuidePreparation::configurationKey(*graph, nodeId);
    }

    return key;
}

std::shared_ptr<const INodeDspConfiguration> NodeDspConfigurationFactory::create(
        AudioModuleRole role,
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model,
        const AudioExecutionSpec&,
        const NodeGraph* graph,
        const String& nodeId) const {
    if (role == AudioModuleRole::MeshSource) {
        return std::shared_ptr<const INodeDspConfiguration>(
                buildTrimeshConfiguration(parameters, model, graph, nodeId));
    }

    using Factory = std::shared_ptr<const INodeDspConfiguration> (*)(
            AudioModuleRole,
            const std::vector<NodeParameter>&,
            const NodeModelStatePtr&);
    struct Registration {
        AudioModuleRole role;
        Factory factory;
    };
    static const Registration registrations[] {
        { AudioModuleRole::ModulationSource, [](AudioModuleRole, const auto& values, const auto&) {
            return std::shared_ptr<const INodeDspConfiguration>(
                    ModulationSource::buildConfiguration(values));
        } },
        { AudioModuleRole::ModulationTriple, [](AudioModuleRole, const auto& values, const auto&) {
            return std::shared_ptr<const INodeDspConfiguration>(
                    buildModulationTripleConfiguration(values));
        } },
        { AudioModuleRole::WaveSource, [](AudioModuleRole roleToUse, const auto& values, const auto&) {
            auto configuration = buildTrimeshConfiguration(values, {}, nullptr, {});
            const NodeParameterMap parameters(values);
            configuration->processorRole = roleToUse;
            configuration->gain = parameters.floatValue("level", 1.f)
                    * parameters.floatValue("amplitude", 1.f);
            return std::shared_ptr<const INodeDspConfiguration>(configuration);
        } },
        { AudioModuleRole::ImageSource, [](AudioModuleRole roleToUse, const auto& values, const auto&) {
            auto configuration = std::make_shared<SourceNodeConfiguration>();
            configuration->processorRole = roleToUse;
            configuration->level = NodeParameterMap(values).floatValue("level", 1.f);
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
            configuration->halfCycleCarry = NodeParameterMap(values).stringValue("mode", "cyclic")
                    == "acyclicCarry";
            return std::shared_ptr<const INodeDspConfiguration>(configuration);
        } },
        { AudioModuleRole::SpectralLayer, [](AudioModuleRole, const auto& values, const auto&) {
            auto configuration = std::make_shared<SpectralLayerConfiguration>();
            const NodeParameterMap parameters(values);
            configuration->pan = parameters.floatValue("pan", 0.5f);
            configuration->range = parameters.floatValue("range", 0.5f);
            configuration->additive = parameters.stringValue("mode", "additive") == "additive";
            return std::shared_ptr<const INodeDspConfiguration>(configuration);
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
            const NodeParameterMap parameters(values);
            configuration->enabled = parameters.boolValue("enabled", true);
            configuration->time = parameters.floatValue("time", 0.5f);
            configuration->feedback = parameters.floatValue("feedback", 0.5f);
            configuration->spin = parameters.floatValue("spin", 1.f);
            configuration->wet = parameters.floatValue("wet", 0.9f);
            configuration->spinIterations = parameters.floatValue("spinIters", 0.f);
            return std::shared_ptr<const INodeDspConfiguration>(configuration);
        } },
        { AudioModuleRole::Equalizer, [](AudioModuleRole, const auto& values, const auto&) {
            return std::shared_ptr<const INodeDspConfiguration>(
                    EqualizerSignalProcessor::buildConfiguration(values));
        } },
        { AudioModuleRole::Unison, [](AudioModuleRole, const auto& values, const auto& modelState) {
            return std::shared_ptr<const INodeDspConfiguration>(
                    buildUnisonNodeConfiguration(values, modelState));
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

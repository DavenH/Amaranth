#include "NodeDefinition.h"

#include "../Nodes/Effect2D/CurveNodeModels.h"

#include <algorithm>
#include <climits>
#include <cmath>

namespace CycleV2 {

namespace {

constexpr auto presentation = ParameterImpact::Presentation;
constexpr auto preview = ParameterImpact::Preview;
constexpr auto graph = ParameterImpact::GraphSemantics;
constexpr auto dsp = ParameterImpact::DspConfiguration;
constexpr auto reset = ParameterImpact::ProcessorReset;

Port input(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout = ChannelLayout::Mono,
        PortPurpose purpose = PortPurpose::Signal,
        PortSide side = PortSide::Left) {
    return { std::move(id), std::move(label), domain, layout, purpose, true, side };
}

Port output(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout = ChannelLayout::Mono,
        PortSide side = PortSide::Right) {
    return { std::move(id), std::move(label), domain, layout, PortPurpose::Signal, false, side };
}

ParameterDefinition boolean(
        String id,
        String label,
        bool defaultValue,
        ParameterImpact impacts) {
    return {
            std::move(id),
            std::move(label),
            ParameterType::Boolean,
            defaultValue ? "1" : "0",
            {},
            impacts
    };
}

ParameterDefinition integer(
        String id,
        String label,
        int defaultValue,
        int minimum,
        int maximum,
        ParameterImpact impacts) {
    return {
            std::move(id),
            std::move(label),
            ParameterType::Integer,
            String(defaultValue),
            { (double) minimum, (double) maximum, {} },
            impacts
    };
}

ParameterDefinition number(
        String id,
        String label,
        float defaultValue,
        float minimum,
        float maximum,
        ParameterImpact impacts) {
    return {
            std::move(id),
            std::move(label),
            ParameterType::Float,
            String(defaultValue),
            { (double) minimum, (double) maximum, {} },
            impacts
    };
}

ParameterDefinition choice(
        String id,
        String label,
        String defaultValue,
        std::initializer_list<const char*> choices,
        ParameterImpact impacts) {
    StringArray values;
    for (const auto* value : choices) {
        values.add(value);
    }
    return {
            std::move(id),
            std::move(label),
            ParameterType::Choice,
            std::move(defaultValue),
            { {}, {}, std::move(values) },
            impacts
    };
}

ParameterDefinition snapshot(String id, String label, String defaultValue) {
    return {
            std::move(id),
            std::move(label),
            ParameterType::ModelSnapshot,
            std::move(defaultValue),
            {},
            preview | dsp
    };
}

NodeDefinition definition(
        String typeId,
        NodeKind kind,
        String displayName,
        String subtitle,
        String prefix,
        std::vector<Port> inputs,
        std::vector<Port> outputs,
        std::vector<ParameterDefinition> parameters = {},
        bool allowsDynamicParameters = false) {
    return {
            std::move(typeId),
            1,
            kind,
            std::move(displayName),
            std::move(subtitle),
            std::move(prefix),
            std::move(inputs),
            std::move(outputs),
            std::move(parameters),
            allowsDynamicParameters
    };
}

class DefinitionBuilder {
public:
    explicit DefinitionBuilder(NodeDefinition definitionToBuild) : value(std::move(definitionToBuild)) {}

    DefinitionBuilder& runtime(
            AudioModuleRole audioRole,
            PreviewModuleRole previewRole,
            String cycle1Reference = {}) {
        value.audioRole = audioRole;
        value.previewRole = previewRole;
        value.previewContract = previewRole == PreviewModuleRole::None
                ? PreviewContract::None
                : (previewRole == PreviewModuleRole::MeshSurface
                        ? PreviewContract::AuthoritativeModel
                        : (previewRole == PreviewModuleRole::SignalSpy
                                ? PreviewContract::RuntimeTap
                                : PreviewContract::Qualitative));
        value.executable = audioRole != AudioModuleRole::None;
        value.previewable = previewRole != PreviewModuleRole::None;
        value.cycle1Reference = std::move(cycle1Reference);
        return *this;
    }

    DefinitionBuilder& disablePreview() {
        value.previewable = false;
        return *this;
    }

    DefinitionBuilder& presentation(
            NodeNaturalSize minimumPreviewSize,
            NodeNaturalSize fixedNaturalSize = {}) {
        value.minimumPreviewSize = minimumPreviewSize;
        value.fixedNaturalSize = fixedNaturalSize;
        return *this;
    }

    NodeDefinition finish() { return std::move(value); }

private:
    NodeDefinition value;
};

DefinitionBuilder buildDefinition(NodeDefinition value) {
    return DefinitionBuilder(std::move(value));
}

bool isBooleanText(const String& value) {
    const String normalized = value.trim().toLowerCase();
    return normalized == "0" || normalized == "1"
            || normalized == "false" || normalized == "true"
            || normalized == "off" || normalized == "on"
            || normalized == "no" || normalized == "yes";
}

}

ParameterImpact operator|(ParameterImpact left, ParameterImpact right) {
    return (ParameterImpact) ((uint32_t) left | (uint32_t) right);
}

bool hasImpact(ParameterImpact impacts, ParameterImpact impact) {
    return ((uint32_t) impacts & (uint32_t) impact) != 0;
}

bool ParameterDefinition::accepts(const String& value) const {
    if (type == ParameterType::Boolean) {
        return isBooleanText(value);
    }

    if (type == ParameterType::Choice) {
        return constraint.choices.contains(value);
    }

    if (type == ParameterType::Integer || type == ParameterType::Float) {
        const double parsed = value.getDoubleValue();
        if (!std::isfinite(parsed)) {
            return false;
        }
        if (constraint.minimum.has_value() && parsed < *constraint.minimum) {
            return false;
        }
        if (constraint.maximum.has_value() && parsed > *constraint.maximum) {
            return false;
        }
    }

    return true;
}

String ParameterDefinition::normalized(const String& value) const {
    if (type == ParameterType::Boolean) {
        const String normalizedValue = value.trim().toLowerCase();
        return normalizedValue == "1" || normalizedValue == "true"
                || normalizedValue == "on" || normalizedValue == "yes"
                ? "1"
                : "0";
    }

    if (type == ParameterType::Integer) {
        return String(value.getIntValue());
    }

    return value;
}

const NodeDefinitionRegistry& NodeDefinitionRegistry::instance() {
    static const NodeDefinitionRegistry registry;
    return registry;
}

NodeDefinitionRegistry::NodeDefinitionRegistry() {
    nodeDefinitions = {
            buildDefinition(definition("genericProcessor", NodeKind::GenericProcessor, "Processor", "generic", "processor",
                    { input("in", "In", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
                    { output("out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }, {}, true))
                    .runtime(AudioModuleRole::GenericProcessor, PreviewModuleRole::Generic)
                    .finish(),
            buildDefinition(definition("voiceContext", NodeKind::VoiceContext, "Voice Context", "waveform start", "voice", {},
                    { output("context", "Context", PortDomain::DomainContext) }, {
                            choice("domain", "Start Domain", "waveform", { "waveform", "spectral", "spectralMagnitude", "spectralPhase" }, graph | presentation | preview),
                            integer("voices", "Voices", 1, 1, 64, dsp),
                            integer("octave", "Octave", 0, -2, 2, dsp | presentation),
                            number("pitch", "Pitch", 0.f, -48.f, 48.f, dsp | presentation),
                            boolean("portamento", "Portamento", false, dsp | presentation),
                            choice("oversampling", "Oversampling", "1x", { "1x", "2x", "4x", "8x" }, dsp | reset | presentation)
                    }))
                    .runtime(AudioModuleRole::VoiceContext, PreviewModuleRole::VoiceContext)
                    .disablePreview()
                    .presentation({}, { 300.f, 128.f })
                    .finish(),
            buildDefinition(definition("waveSource", NodeKind::WaveSource, "Wave", "time source", "wave",
                    { input("context", "Context", PortDomain::DomainContext) },
                    { output("out", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }, {
                            number("level", "Level", 1.f, 0.f, 1.f, dsp | preview),
                            number("amplitude", "Amplitude", 1.f, 0.f, 1.f, preview)
                    }))
                    .runtime(AudioModuleRole::WaveSource, PreviewModuleRole::Waveform)
                    .presentation({ 220.f, 90.f })
                    .finish(),
            buildDefinition(definition("imageSource", NodeKind::ImageSource, "Image", "raster source", "image",
                    { input("context", "Context", PortDomain::DomainContext) },
                    { output("out", "Out", PortDomain::ControlSignal, ChannelLayout::LinkedStereo) }, {
                            number("level", "Level", 1.f, 0.f, 1.f, dsp | preview)
                    }))
                    .runtime(AudioModuleRole::ImageSource, PreviewModuleRole::Image)
                    .presentation({ 220.f, 90.f })
                    .finish(),
            buildDefinition(definition("trilinearMesh", NodeKind::TrilinearMesh, "Trilinear Mesh", "mesh operand", "mesh",
                    { input("context", "Context", PortDomain::DomainContext),
                      input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment),
                      input("yellow", "Yellow Morph", PortDomain::ControlSignal),
                      input("red", "Red Morph", PortDomain::ControlSignal),
                      input("blue", "Blue Morph", PortDomain::ControlSignal) },
                    { output("out", "Out", PortDomain::ControlSignal, ChannelLayout::LinkedStereo) }, {
                            number("yellow", "Yellow", 0.5f, 0.f, 1.f, dsp | preview | presentation),
                            number("red", "Red", 0.5f, 0.f, 1.f, dsp | preview | presentation),
                            number("blue", "Blue", 0.5f, 0.f, 1.f, dsp | preview | presentation),
                            choice("primaryAxis", "Primary Axis", "yellow", { "yellow", "red", "blue" }, preview | presentation)
                    }, true))
                    .runtime(AudioModuleRole::MeshSource, PreviewModuleRole::MeshSurface,
                            "cycle/src/Curve/Rasterization/Rasterizer/VoiceMeshRasterizer.cpp")
                    .presentation({ 260.f, 130.f }, { 286.f, 269.f })
                    .finish(),
            buildDefinition(definition("fft", NodeKind::Fft, String::fromUTF8("Time → Freq"), "cycle chunks", "fft",
                    { input("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
                    { output("mag", "Mag", PortDomain::SpectralMagnitudeSignal), output("phase", "Phase", PortDomain::SpectralPhaseSignal) }, {
                            integer("cycleFrames", "Cycle Frames", 2048, 1, 65536, dsp | reset),
                            choice("mode", "Mode", "cycle", { "cycle", "fixedWindow" }, graph | dsp | presentation)
                    }))
                    .runtime(AudioModuleRole::Fft, PreviewModuleRole::None)
                    .presentation({}, { 278.f, 178.f })
                    .finish(),
            buildDefinition(definition("ifft", NodeKind::Ifft, String::fromUTF8("Freq → Time"), "cyclic overlap", "ifft",
                    { input("mag", "Mag", PortDomain::SpectralMagnitudeSignal), input("phase", "Phase", PortDomain::SpectralPhaseSignal) },
                    { output("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }, {
                            integer("cycleFrames", "Cycle Frames", 2048, 1, 65536, dsp | reset),
                            choice("mode", "Mode", "cyclic", { "cyclic", "acyclicCarry" }, graph | dsp | presentation)
                    }))
                    .runtime(AudioModuleRole::Ifft, PreviewModuleRole::None)
                    .presentation({}, { 278.f, 178.f })
                    .finish(),
            buildDefinition(definition("envelope", NodeKind::Envelope, "Envelope", "control curve", "env",
                    { input("red", "Red Morph", PortDomain::ControlSignal),
                      input("blue", "Blue Morph", PortDomain::ControlSignal) },
                    { output("env", "Env", PortDomain::EnvelopeSignal) }, {
                            boolean("logarithmic", "Logarithmic", false, dsp | preview | presentation),
                            snapshot(CurveNodeModelCodec::snapshotParameterId(), "Curve Model Snapshot",
                                    CurveNodeModelCodec::defaultSnapshot(NodeKind::Envelope)),
                            integer(CurveNodeModelCodec::revisionParameterId(), "Curve Model Revision", 1, 1, INT_MAX, dsp | preview),
                            number("red", "Red", 0.5f, 0.f, 1.f, dsp | preview | presentation),
                            number("blue", "Blue", 0.5f, 0.f, 1.f, dsp | preview | presentation),
                            boolean("dynamic", "Dynamic While Live", false, dsp | presentation),
                            number("level", "Level", 1.f, 0.f, 1.f, dsp)
                    }, true))
                    .runtime(AudioModuleRole::Envelope, PreviewModuleRole::Envelope,
                            "cycle/src/Inter/EnvelopeInter2D.cpp")
                    .presentation({ 269.2f, 92.f })
                    .finish(),
            buildDefinition(definition("add", NodeKind::Add, "Add", "combine", "add",
                    { input("left", "A", PortDomain::ControlSignal), input("right", "B", PortDomain::ControlSignal) },
                    { output("out", "Out", PortDomain::ControlSignal) }))
                    .runtime(AudioModuleRole::Add, PreviewModuleRole::None)
                    .presentation({ 58.f, 44.f }, { 150.f, 118.f })
                    .finish(),
            buildDefinition(definition("multiply", NodeKind::Multiply, "Multiply", "operation", "multiply",
                    { input("left", "A", PortDomain::ControlSignal), input("right", "B", PortDomain::ControlSignal) },
                    { output("out", "Out", PortDomain::ControlSignal) }))
                    .runtime(AudioModuleRole::Multiply, PreviewModuleRole::None)
                    .presentation({ 58.f, 44.f }, { 150.f, 118.f })
                    .finish(),
            buildDefinition(definition("guideCurve", NodeKind::GuideCurve, "Guide", "mesh attachment", "guide", {},
                    { output("guide", "Guide", PortDomain::EnvelopeSignal) }, {
                            boolean("enabled", "Enabled", true, dsp | preview | presentation),
                            number("noise", "Noise", 0.5f, 0.f, 1.f, dsp | preview | presentation),
                            number("dcOffset", "DC Offset", 0.5f, 0.f, 1.f, dsp | preview | presentation),
                            number("phase", "Phase", 0.5f, 0.f, 1.f, dsp | preview | presentation),
                            snapshot(CurveNodeModelCodec::snapshotParameterId(), "Curve Model Snapshot",
                                    CurveNodeModelCodec::defaultSnapshot(NodeKind::GuideCurve)),
                            integer(CurveNodeModelCodec::revisionParameterId(), "Curve Model Revision", 1, 1, INT_MAX, dsp | preview)
                    }, true))
                    .runtime(AudioModuleRole::GuideCurve, PreviewModuleRole::Envelope,
                            "cycle/src/UI/VertexPanels/GuideCurvePanel.cpp")
                    .presentation({ 269.2f, 100.f })
                    .finish(),
            buildDefinition(definition("impulseResponse", NodeKind::ImpulseResponse, "IR", "convolution", "ir",
                    { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
                    { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }, {
                            boolean("enabled", "Enabled", true, dsp | presentation),
                            number("size", "Size", 0.5f, 0.f, 1.f, dsp | preview | presentation),
                            number("post", "Post", 0.5f, 0.f, 1.f, dsp | presentation),
                            number("highPass", "HighPass", 0.5f, 0.f, 1.f, dsp | presentation),
                            snapshot(CurveNodeModelCodec::snapshotParameterId(), "Curve Model Snapshot",
                                    CurveNodeModelCodec::defaultSnapshot(NodeKind::ImpulseResponse)),
                            integer(CurveNodeModelCodec::revisionParameterId(), "Curve Model Revision", 1, 1, INT_MAX, dsp | preview)
                    }, true))
                    .runtime(AudioModuleRole::ImpulseResponse, PreviewModuleRole::ImpulseResponse,
                            "cycle/src/Audio/Effects/IrModeller.cpp")
                    .presentation({ 230.f, 92.f })
                    .finish(),
            buildDefinition(definition("waveshaper", NodeKind::Waveshaper, "Waveshaper", "transfer curve", "waveshaper",
                    { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
                    { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }, {
                            boolean("enabled", "Enabled", true, dsp | presentation),
                            number("pre", "Pre", 0.5f, 0.f, 1.f, dsp | presentation),
                            number("post", "Post", 0.5f, 0.f, 1.f, dsp | presentation),
                            choice("aaFactor", "AA Factor", "1", { "1", "2", "4", "8" }, dsp | reset | presentation),
                            snapshot(CurveNodeModelCodec::snapshotParameterId(), "Curve Model Snapshot",
                                    CurveNodeModelCodec::defaultSnapshot(NodeKind::Waveshaper)),
                            integer(CurveNodeModelCodec::revisionParameterId(), "Curve Model Revision", 1, 1, INT_MAX, dsp | preview)
                    }, true))
                    .runtime(AudioModuleRole::Waveshaper, PreviewModuleRole::Waveshaper,
                            "cycle/src/Audio/Effects/WaveShaper.cpp")
                    .presentation({ 154.f, 174.f })
                    .finish(),
            buildDefinition(definition("reverb", NodeKind::Reverb, "Reverb", "space", "reverb",
                    { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
                    { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }, {
                            boolean("enabled", "Enabled", true, dsp | presentation),
                            number("size", "Size", 0.35f, 0.f, 1.f, dsp | presentation),
                            number("damp", "Damp", 0.25f, 0.f, 1.f, dsp | presentation),
                            number("width", "Width", 0.75f, 0.f, 1.f, dsp | presentation),
                            number("wet", "Wet", 0.35f, 0.f, 1.f, dsp | presentation),
                            number("highPass", "HighPass", 0.05f, 0.f, 1.f, dsp)
                    }))
                    .runtime(AudioModuleRole::Reverb, PreviewModuleRole::None,
                            "cycle/src/Audio/Effects/Reverb.cpp")
                    .finish(),
            buildDefinition(definition("delay", NodeKind::Delay, "Delay", "echo", "delay",
                    { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
                    { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }, {
                            boolean("enabled", "Enabled", true, dsp | presentation),
                            number("time", "Time", 0.25f, 0.f, 1.f, dsp | presentation),
                            number("feedback", "Feedback", 0.25f, 0.f, 1.f, dsp | presentation),
                            number("wet", "Wet", 0.5f, 0.f, 1.f, dsp | presentation),
                            number("spin", "Spin", 1.f, 0.f, 1.f, dsp),
                            integer("spinIters", "Spin Iterations", 0, 0, 16, dsp)
                    }))
                    .runtime(AudioModuleRole::Delay, PreviewModuleRole::None,
                            "cycle/src/Audio/Effects/Delay.cpp")
                    .finish(),
            buildDefinition(definition("spy", NodeKind::Spy, "Spy", "signal monitor", "spy",
                    { input("in", "In", PortDomain::ControlSignal) }, {}))
                    .runtime(AudioModuleRole::Spy, PreviewModuleRole::SignalSpy)
                    .presentation({ 170.f, 76.f })
                    .finish(),
            buildDefinition(definition("stereoSplit", NodeKind::StereoSplit, "Stereo Split", "L/R breakout", "split",
                    { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
                    { output("left", "Left", PortDomain::TimeSignal, ChannelLayout::Left), output("right", "Right", PortDomain::TimeSignal, ChannelLayout::Right) }))
                    .runtime(AudioModuleRole::StereoSplit, PreviewModuleRole::None)
                    .finish(),
            buildDefinition(definition("stereoJoin", NodeKind::StereoJoin, "Stereo Join", "L/R combine", "join",
                    { input("left", "Left", PortDomain::TimeSignal, ChannelLayout::Left), input("right", "Right", PortDomain::TimeSignal, ChannelLayout::Right) },
                    { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }))
                    .runtime(AudioModuleRole::StereoJoin, PreviewModuleRole::None)
                    .finish(),
            buildDefinition(definition("output", NodeKind::Output, "Output", "sink", "out",
                    { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }, {}))
                    .runtime(AudioModuleRole::Output, PreviewModuleRole::None)
                    .presentation({}, { 190.f, 160.f })
                    .finish()
    };
}

const NodeDefinition* NodeDefinitionRegistry::find(NodeKind kind) const {
    for (const auto& definitionToCheck : nodeDefinitions) {
        if (definitionToCheck.kind == kind) {
            return &definitionToCheck;
        }
    }
    return nullptr;
}
const NodeDefinition* NodeDefinitionRegistry::find(const String& typeId) const {
    for (const auto& definitionToCheck : nodeDefinitions) {
        if (definitionToCheck.typeId == typeId) {
            return &definitionToCheck;
        }
    }
    return nullptr;
}

const ParameterDefinition* NodeDefinitionRegistry::findParameter(
        NodeKind kind,
        const String& parameterId) const {
    const auto* definitionToSearch = find(kind);
    if (definitionToSearch == nullptr) {
        return nullptr;
    }
    for (const auto& parameter : definitionToSearch->parameters) {
        if (parameter.id == parameterId) {
            return &parameter;
        }
    }
    return nullptr;
}

String NodeDefinitionRegistry::typeIdFor(NodeKind kind) const {
    const auto* definitionToUse = find(kind);
    return definitionToUse != nullptr ? definitionToUse->typeId : "genericProcessor";
}

NodeKind NodeDefinitionRegistry::kindForTypeId(const String& typeId) const {
    const auto* definitionToUse = find(typeId);
    return definitionToUse != nullptr ? definitionToUse->kind : NodeKind::GenericProcessor;
}

void NodeDefinitionRegistry::normalize(Node& node) const {
    const auto* definitionToUse = find(node.kind);
    if (definitionToUse == nullptr) {
        return;
    }

    node.title = definitionToUse->displayName;
    if (node.subtitle.isEmpty()) {
        node.subtitle = definitionToUse->subtitle;
    }
    for (const auto& canonicalInput : definitionToUse->inputs) {
        auto existing = std::find_if(node.inputs.begin(), node.inputs.end(), [&](const auto& input) {
            return input.id == canonicalInput.id;
        });
        if (existing == node.inputs.end()) {
            node.inputs.push_back(canonicalInput);
        } else if ((node.kind == NodeKind::Envelope || node.kind == NodeKind::TrilinearMesh)
                && (canonicalInput.id == "yellow" || canonicalInput.id == "red" || canonicalInput.id == "blue")) {
            existing->side = canonicalInput.side;
        }
    }
    if (node.outputs.empty() && !definitionToUse->outputs.empty()) {
        node.outputs = definitionToUse->outputs;
    }

    for (const auto& parameterDefinition : definitionToUse->parameters) {
        auto found = std::find_if(node.parameters.begin(), node.parameters.end(), [&](const auto& parameter) {
            return parameter.id == parameterDefinition.id;
        });
        if (found == node.parameters.end()) {
            node.parameters.push_back({
                    parameterDefinition.id,
                    parameterDefinition.label,
                    parameterDefinition.defaultValue
            });
            continue;
        }
        found->label = parameterDefinition.label;
        if (found->value.isEmpty() && parameterDefinition.type == ParameterType::ModelSnapshot) {
            found->value = parameterDefinition.defaultValue;
        } else if (parameterDefinition.accepts(found->value)) {
            found->value = parameterDefinition.normalized(found->value);
        }
    }
}

String typedParameterString(
        const std::vector<NodeParameter>& parameters,
        const String& parameterId,
        const String& fallback) {
    for (const auto& parameter : parameters) {
        if (parameter.id == parameterId) {
            return parameter.value;
        }
    }
    return fallback;
}

bool typedParameterBool(
        const std::vector<NodeParameter>& parameters,
        const String& parameterId,
        bool fallback) {
    const String value = typedParameterString(parameters, parameterId, fallback ? "1" : "0").toLowerCase();
    return value == "1" || value == "true" || value == "on" || value == "yes";
}

int typedParameterInt(
        const std::vector<NodeParameter>& parameters,
        const String& parameterId,
        int fallback) {
    const String value = typedParameterString(parameters, parameterId);
    return value.isNotEmpty() ? value.getIntValue() : fallback;
}

float typedParameterFloat(
        const std::vector<NodeParameter>& parameters,
        const String& parameterId,
        float fallback) {
    const String value = typedParameterString(parameters, parameterId);
    return value.isNotEmpty() ? value.getFloatValue() : fallback;
}

}

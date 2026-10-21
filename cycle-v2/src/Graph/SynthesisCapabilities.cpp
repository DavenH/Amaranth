#include <algorithm>
#include <cmath>
#include <utility>

#include "Graph/SynthesisCapabilities.h"

namespace CycleV2 {

namespace {

var object() {
    return var(new DynamicObject());
}

String timingName(ControlTiming timing) {
    switch (timing) {
        case ControlTiming::Continuous:      return "continuous";
        case ControlTiming::NoteOnLatched:   return "noteOnLatched";
        case ControlTiming::StaticAuthoring: return "staticAuthoring";
        case ControlTiming::Unspecified:     return "unspecified";
    }
    return "unspecified";
}

var mappingContext(const ParameterMappingContext& context) {
    auto result = object();
    result.getDynamicObject()->setProperty("sampleRate", context.sampleRate);
    result.getDynamicObject()->setProperty("beatsPerMeasure", context.beatsPerMeasure);
    return result;
}

var ports(const std::vector<Port>& definitions) {
    Array<var> result;
    for (const auto& port : definitions) {
        auto value = object();
        auto& fields = *value.getDynamicObject();
        fields.setProperty("id", port.id);
        fields.setProperty("label", port.label);
        fields.setProperty("domain", labelForDomain(port.domain));
        fields.setProperty("channelLayout", labelForChannelLayout(port.channelLayout));
        fields.setProperty("connectionKind", port.connectionKind == ConnectionKind::Signal
                ? "signal" : (port.connectionKind == ConnectionKind::ConfigurationAttachment
                        ? "configurationAttachment" : "processingAttachment"));
        const char* attachmentNames[] { "none", "scratchEnvelope", "modulationTriple", "unison" };
        fields.setProperty("attachmentType", attachmentNames[(int) port.attachmentType]);
        const char* slotNames[] { "none", "yellow", "red", "blue" };
        fields.setProperty("defaultModulationSlot", slotNames[(int) port.defaultModulationSlot]);
        fields.setProperty("controlTiming", timingName(port.controlTiming));
        result.add(value);
    }
    return result;
}

var parameterJSON(const ParameterDefinition& parameter, const ParameterMappingContext& context) {
    auto result = object();
    auto& fields = *result.getDynamicObject();
    const char* typeNames[] { "boolean", "integer", "float", "choice", "text" };
    fields.setProperty("id", parameter.id);
    fields.setProperty("label", parameter.label);
    fields.setProperty("type", typeNames[(int) parameter.type]);
    fields.setProperty("defaultValue", parameter.defaultValue);
    fields.setProperty("persisted", parameter.persisted);
    fields.setProperty("controlTiming", timingName(parameter.controlTiming));
    fields.setProperty("normalizesValues", (bool) parameter.normalizer);
    if (parameter.constraint.minimum.has_value()) {
        fields.setProperty("minimum", *parameter.constraint.minimum);
    }
    if (parameter.constraint.maximum.has_value()) {
        fields.setProperty("maximum", *parameter.constraint.maximum);
    }
    fields.setProperty("choices", var(parameter.constraint.choices));
    Array<var> impacts;
    for (const auto& impact : {
            std::pair { ParameterImpact::Presentation, "presentation" },
            std::pair { ParameterImpact::Preview, "preview" },
            std::pair { ParameterImpact::GraphSemantics, "graphSemantics" },
            std::pair { ParameterImpact::DspConfiguration, "dspConfiguration" },
            std::pair { ParameterImpact::ProcessorReset, "processorReset" } }) {
        if (hasImpact(parameter.impacts, impact.first)) {
            impacts.add(impact.second);
        }
    }
    fields.setProperty("impacts", impacts);
    if (parameter.physicalMapping.has_value()) {
        const auto& mapping = *parameter.physicalMapping;
        auto physical = object();
        auto& physicalFields = *physical.getDynamicObject();
        physicalFields.setProperty("id", mapping.id);
        physicalFields.setProperty("unit", mapping.unit);
        physicalFields.setProperty("valueType", mapping.integralPhysicalValue ? "integer" : "float");
        physicalFields.setProperty("inverseAvailable", (bool) mapping.toStored);
        physicalFields.setProperty("defaultValue", mapping.toPhysical(
                parameter.defaultValue.getDoubleValue(), context));
        Array<var> examples;
        if (parameter.constraint.minimum.has_value() && parameter.constraint.maximum.has_value()) {
            for (int step = 0; step <= 4; ++step) {
                const double stored = *parameter.constraint.minimum
                        + (*parameter.constraint.maximum - *parameter.constraint.minimum) * step / 4.0;
                auto example = object();
                example.getDynamicObject()->setProperty("storedValue", stored);
                example.getDynamicObject()->setProperty("physicalValue", mapping.toPhysical(stored, context));
                examples.add(example);
            }
        }
        physicalFields.setProperty("examples", examples);
        fields.setProperty("physicalMapping", physical);
    }
    return result;
}

}

var SynthesisCapabilities::exportJSON(const ParameterMappingContext& context) {
    if (!context.isValid()) {
        return {};
    }
    auto result = object();
    auto& fields = *result.getDynamicObject();
    fields.setProperty("schema", "cycle-v2-synthesis-capabilities.v1");
    fields.setProperty("mappingContext", mappingContext(context));
    fields.setProperty("guidance", var(StringArray {
            "Stored parameter values are strings; physical values use declared conversions, not interpolation of examples.",
            "Use applyNodeEdits for atomic stored parameter/model edits and connectPorts for connections; domain validation still applies.",
            "staticAuthoring describes stored configuration, not a continuous performance-modulation input.",
            "Continuous morph controls update at the configured control interval, not necessarily each audio sample.",
            "Envelope morph inputs latch at note-on by default. Voice Context envelopeModulation=live adopts held-note changes at audio-block boundaries and freezes geometry after release.",
            "Voice Context noteMode=legato preserves overlapping-note traversal per MIDI source/channel; portamento glides those transitions over portamentoSeconds. Only one Voice Context may be active in a graph.",
            "Modulation-source output depends on source selection; velocity/key are note properties, CC/pressure are live controls.",
            "Model defaults are codec examples. Authored replacement is available only when model.authoringReplacement is true; it validates structure, not musical quality.",
            "Connection domains can specialize with graph context and signalType; exported ports are declared defaults.",
            "Unknown physical mappings, control timing and perceptual effects are unspecified; no universal brightness control is promised."
    }));
    Array<var> nodes;
    for (const auto& definition : NodeDefinitionRegistry::instance().definitions()) {
        auto node = object();
        auto& nodeFields = *node.getDynamicObject();
        nodeFields.setProperty("typeId", definition.typeId);
        nodeFields.setProperty("version", definition.version);
        nodeFields.setProperty("displayName", definition.displayName);
        nodeFields.setProperty("help", definition.helpText);
        nodeFields.setProperty("inputs", ports(definition.inputs));
        nodeFields.setProperty("outputs", ports(definition.outputs));
        nodeFields.setProperty("allowsDynamicParameters", definition.allowsDynamicParameters);
        nodeFields.setProperty("requiredSingleton", definition.requiredSingleton);
        nodeFields.setProperty("removable", definition.removable);
        nodeFields.setProperty("executable", definition.executable);
        nodeFields.setProperty("previewable", definition.previewable);
        const char* traits[] {
                "configurationOnly", "controlProducer", "cycleGenerator", "coordinateTransform",
                "spectralTransform", "oscillatorMaterializer", "sampleBlockProcessor"
        };
        nodeFields.setProperty("executionTrait", traits[(int) definition.executionTrait]);
        const char* scopes[] { "voice", "global" };
        const char* capabilities[] { "voiceOnly", "globalOnly", "selectable", "domainNeutral" };
        nodeFields.setProperty("processingScope", scopes[(int) definition.processingScope]);
        nodeFields.setProperty("processingCapability", capabilities[(int) definition.processingCapability]);
        Array<var> parameters;
        for (const auto& parameter : definition.parameters) {
            parameters.add(parameterJSON(parameter, context));
        }
        nodeFields.setProperty("parameters", parameters);
        if (definition.modelCodec != nullptr) {
            auto model = object();
            model.getDynamicObject()->setProperty("schemaId", definition.modelCodec->schemaId());
            model.getDynamicObject()->setProperty("version", definition.modelCodec->currentVersion());
            model.getDynamicObject()->setProperty("authoringReplacement", definition.modelCodec->supportsAuthoring());
            model.getDynamicObject()->setProperty("default", definition.modelCodec->createDefault()->writeJSON());
            nodeFields.setProperty("model", model);
        }
        nodes.add(node);
    }
    fields.setProperty("nodes", nodes);
    return result;
}

var SynthesisCapabilities::convertParameter(
        const String& typeId,
        const String& parameterId,
        double value,
        bool toStored,
        const ParameterMappingContext& context,
        String& error) {
    error.clear();
    const auto* definition = NodeDefinitionRegistry::instance().find(typeId);
    const auto* parameter = definition != nullptr
            ? NodeDefinitionRegistry::instance().findParameter(definition->kind, parameterId) : nullptr;
    if (parameter == nullptr || !parameter->physicalMapping.has_value()) {
        error = "Unknown type/parameter or no physical mapping is declared";
        return {};
    }
    const auto& mapping = *parameter->physicalMapping;
    if (!std::isfinite(value) || !context.isValid()) {
        error = "Conversion requires a finite value, positive sample rate and positive beats per measure";
        return {};
    }
    if (toStored && mapping.integralPhysicalValue && std::trunc(value) != value) {
        error = "Physical value must be an integer";
        return {};
    }
    if (toStored && !mapping.toStored) {
        error = "No inverse conversion is available";
        return {};
    }
    if (toStored && parameter->constraint.minimum.has_value() && parameter->constraint.maximum.has_value()) {
        const double a = mapping.toPhysical(*parameter->constraint.minimum, context);
        const double b = mapping.toPhysical(*parameter->constraint.maximum, context);
        if (value < std::min(a, b) || value > std::max(a, b)) {
            error = "Physical value is outside the declared parameter range";
            return {};
        }
    }
    const double stored = toStored ? mapping.toStored(value, context) : value;
    if (!std::isfinite(stored)
            || (parameter->constraint.minimum.has_value() && stored < *parameter->constraint.minimum)
            || (parameter->constraint.maximum.has_value() && stored > *parameter->constraint.maximum)
            || !parameter->accepts(String(stored, 12))) {
        error = "Stored value is outside the declared parameter range";
        return {};
    }
    const String normalized = parameter->normalized(String(stored, 12));
    auto result = object();
    auto& fields = *result.getDynamicObject();
    fields.setProperty("typeId", typeId);
    fields.setProperty("parameterId", parameterId);
    fields.setProperty("inputValue", value);
    fields.setProperty("storedValue", normalized);
    fields.setProperty("physicalValue", mapping.toPhysical(normalized.getDoubleValue(), context));
    fields.setProperty("unit", mapping.unit);
    fields.setProperty("mappingContext", mappingContext(context));
    return result;
}

}

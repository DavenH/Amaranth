#include "GraphSerializer.h"

#include "GraphNodeFactory.h"
#include "GraphValidator.h"
#include "NodeDefinition.h"

#include <cmath>
#include <unordered_set>

namespace CycleV2 {

namespace {

constexpr auto formatId = "cycle-v2-graph";
constexpr int maximumDecimalPlaces = 5;
constexpr int maximumLineLength = 140;

struct StringHash {
    size_t operator()(const String& value) const {
        return (size_t) value.hashCode64();
    }
};

var parameterToJSON(const NodeParameter& parameter, const ParameterDefinition& definition) {
    switch (definition.type) {
        case ParameterType::Boolean: return parameter.value.getIntValue() != 0;
        case ParameterType::Integer: return parameter.value.getIntValue();
        case ParameterType::Float:   return parameter.value.getDoubleValue();
        default:                     return parameter.value;
    }
}

bool parameterFromJSON(
        const var& value,
        const ParameterDefinition& definition,
        String& result) {
    switch (definition.type) {
        case ParameterType::Boolean:
            if (!value.isBool()) {
                return false;
            }
            result = (bool) value ? "1" : "0";
            return true;
        case ParameterType::Integer:
            if (!value.isInt() && !value.isInt64()) {
                return false;
            }
            result = String((int64) value);
            break;
        case ParameterType::Float: {
            if (!value.isDouble() && !value.isInt() && !value.isInt64()) {
                return false;
            }
            const double number = (double) value;
            if (!std::isfinite(number)) {
                return false;
            }
            result = String(number, 15).trimCharactersAtEnd("0").trimCharactersAtEnd(".");
            if (result == "-0") {
                result = "0";
            }
            break;
        }
        default:
            if (!value.isString()) {
                return false;
            }
            result = value.toString();
            break;
    }
    return definition.accepts(result);
}

var positionToJSON(Point<float> position) {
    auto result = std::make_unique<DynamicObject>();
    result->setProperty("x", position.x);
    result->setProperty("y", position.y);
    return var(result.release());
}

var edgeToJSON(const Edge& edge) {
    auto result = std::make_unique<DynamicObject>();
    result->setProperty("sourceNodeId", edge.sourceNodeId);
    result->setProperty("sourcePortId", edge.sourcePortId);
    result->setProperty("destNodeId", edge.destNodeId);
    result->setProperty("destPortId", edge.destPortId);
    return var(result.release());
}

var probeToJSON(const SignalProbe& probe) {
    auto result = std::make_unique<DynamicObject>();
    result->setProperty("id", probe.id);
    result->setProperty("sourceNodeId", probe.sourceNodeId);
    result->setProperty("sourcePortId", probe.sourcePortId);
    result->setProperty("anchorDestNodeId", probe.anchorDestNodeId);
    result->setProperty("anchorDestPortId", probe.anchorDestPortId);
    result->setProperty("label", probe.label);
    result->setProperty("tapPosition", probe.tapPosition);
    result->setProperty("railOrder", probe.railOrder);
    return var(result.release());
}

const Port* findPort(const Node& node, const String& id, bool input) {
    const auto& ports = input ? node.inputs : node.outputs;
    const auto found = std::find_if(ports.begin(), ports.end(), [&](const auto& port) {
        return port.id == id;
    });
    return found != ports.end() ? &*found : nullptr;
}

PortDomain resolvedEdgeDomain(const Port& source, const Port& destination) {
    return source.domain == PortDomain::ControlSignal
            && destination.domain != PortDomain::ControlSignal
            ? destination.domain
            : source.domain;
}

bool readRequiredString(const DynamicObject& object, const Identifier& name, String& result) {
    const var value = object.getProperty(name);
    if (!value.isString() || value.toString().isEmpty()) {
        return false;
    }
    result = value.toString();
    return true;
}

bool isScalarJSON(const var& value) {
    return value.getDynamicObject() == nullptr && value.getArray() == nullptr;
}

String scalarToJSON(const var& value) {
    return JSON::toString(value, true, maximumDecimalPlaces);
}

String singleLineObject(const DynamicObject& object) {
    String result { "{ " };
    bool first = true;
    for (const auto& property : object.getProperties()) {
        if (!isScalarJSON(property.value)) {
            return {};
        }
        if (!first) {
            result << ", ";
        }
        result << scalarToJSON(property.name.toString()) << ": " << scalarToJSON(property.value);
        first = false;
    }
    return first ? String("{}") : result + " }";
}

void appendIndent(String& output, int depth) {
    output << String::repeatedString("    ", depth);
}

void appendCanonicalJSON(const var& value, int depth, String& output);

void appendCanonicalObject(const DynamicObject& object, int depth, String& output) {
    const String compact = singleLineObject(object);
    if (compact.isNotEmpty() && depth * 4 + compact.length() <= maximumLineLength) {
        output << compact;
        return;
    }

    output << "{\n";
    const auto& properties = object.getProperties();
    int index = 0;
    for (const auto& property : properties) {
        appendIndent(output, depth + 1);
        output << scalarToJSON(property.name.toString()) << ": ";
        appendCanonicalJSON(property.value, depth + 1, output);
        output << (++index < properties.size() ? ",\n" : "\n");
    }
    appendIndent(output, depth);
    output << "}";
}

void appendCanonicalArray(const Array<var>& values, int depth, String& output) {
    if (values.isEmpty()) {
        output << "[]";
        return;
    }

    output << "[\n";
    for (int index = 0; index < values.size(); ++index) {
        appendIndent(output, depth + 1);
        appendCanonicalJSON(values[index], depth + 1, output);
        output << (index + 1 < values.size() ? ",\n" : "\n");
    }
    appendIndent(output, depth);
    output << "]";
}

void appendCanonicalJSON(const var& value, int depth, String& output) {
    if (const auto* object = value.getDynamicObject()) {
        appendCanonicalObject(*object, depth, output);
    } else if (const auto* array = value.getArray()) {
        appendCanonicalArray(*array, depth, output);
    } else {
        output << scalarToJSON(value);
    }
}

}

var GraphSerializer::writeJSON(const NodeGraph& graph) const {
    auto root = std::make_unique<DynamicObject>();
    root->setProperty("format", formatId);
    root->setProperty("formatVersion", currentFormatVersion);

    Array<var> nodes;
    const auto& registry = NodeDefinitionRegistry::instance();
    for (const auto& node : graph.getNodes()) {
        const auto* definition = registry.find(node.kind);
        auto encoded = std::make_unique<DynamicObject>();
        encoded->setProperty("id", node.id);
        encoded->setProperty("kind", registry.typeIdFor(node.kind));
        encoded->setProperty("definitionVersion", definition != nullptr ? definition->version : 1);
        encoded->setProperty("title", node.title);
        encoded->setProperty("position", positionToJSON(node.bounds.getPosition()));

        auto parameters = std::make_unique<DynamicObject>();
        if (definition != nullptr) {
            for (const auto& parameterDefinition : definition->parameters) {
                if (parameterDefinition.persisted) {
                    const auto found = std::find_if(
                            node.parameters.begin(),
                            node.parameters.end(),
                            [&](const auto& parameter) {
                                return parameter.id == parameterDefinition.id;
                            });
                    const NodeParameter parameter = found != node.parameters.end()
                            ? *found
                            : NodeParameter {
                                    parameterDefinition.id,
                                    parameterDefinition.label,
                                    parameterDefinition.defaultValue
                            };
                    parameters->setProperty(
                            parameter.id,
                            parameterToJSON(parameter, parameterDefinition));
                }
            }
        }
        encoded->setProperty("parameters", var(parameters.release()));
        if (node.model != nullptr) {
            encoded->setProperty("model", node.model->writeJSON());
        }
        if (node.editorState.getDynamicObject() != nullptr) {
            encoded->setProperty("editor", node.editorState);
        }
        nodes.add(var(encoded.release()));
    }
    root->setProperty("nodes", var(std::move(nodes)));

    Array<var> edges;
    for (const auto& edge : graph.getEdges()) {
        edges.add(edgeToJSON(edge));
    }
    root->setProperty("edges", var(std::move(edges)));

    Array<var> probes;
    for (const auto& probe : graph.getSignalProbes()) {
        probes.add(probeToJSON(probe));
    }
    root->setProperty("probes", var(std::move(probes)));
    return var(root.release());
}

GraphLoadResult GraphSerializer::readJSON(const var& value) const {
    GraphLoadResult result;
    const auto* root = value.getDynamicObject();
    if (root == nullptr || root->getProperty("format").toString() != formatId) {
        result.issues.push_back({ GraphLoadCode::InvalidSchema, "Root object is not a Cycle V2 graph" });
        return result;
    }
    if ((int) root->getProperty("formatVersion") != currentFormatVersion) {
        result.issues.push_back({ GraphLoadCode::UnsupportedVersion, "Unsupported Cycle V2 graph format version" });
        return result;
    }

    const auto* encodedNodes = root->getProperty("nodes").getArray();
    const auto* encodedEdges = root->getProperty("edges").getArray();
    const auto* encodedProbes = root->getProperty("probes").getArray();
    if (encodedNodes == nullptr || encodedEdges == nullptr || encodedProbes == nullptr) {
        result.issues.push_back({ GraphLoadCode::InvalidSchema, "Graph nodes, edges, and probes must be arrays" });
        return result;
    }

    const auto& registry = NodeDefinitionRegistry::instance();
    std::unordered_set<String, StringHash> nodeIds;
    for (const auto& encodedValue : *encodedNodes) {
        const auto* encoded = encodedValue.getDynamicObject();
        String nodeId;
        String kindId;
        if (encoded == nullptr
                || !readRequiredString(*encoded, "id", nodeId)
                || !readRequiredString(*encoded, "kind", kindId)) {
            result.issues.push_back({ GraphLoadCode::InvalidSchema, "Node identity and kind must be non-empty strings" });
            continue;
        }
        if (!nodeIds.emplace(nodeId).second) {
            result.issues.push_back({ GraphLoadCode::DuplicateIdentity, "Duplicate node identity '" + nodeId + "'" });
            continue;
        }
        const auto* definition = registry.find(kindId);
        if (definition == nullptr) {
            result.issues.push_back({ GraphLoadCode::UnknownNodeType, "Unknown node kind '" + kindId + "'" });
            continue;
        }
        if ((int) encoded->getProperty("definitionVersion") != definition->version) {
            result.issues.push_back({ GraphLoadCode::UnsupportedVersion, "Unsupported definition version for node '" + nodeId + "'" });
            continue;
        }

        Node node = GraphNodeFactory().createNode(definition->kind, nodeId, {});
        const var title = encoded->getProperty("title");
        if (title.isString() && title.toString().isNotEmpty()) {
            node.title = title.toString();
        }
        const auto* position = encoded->getProperty("position").getDynamicObject();
        if (position == nullptr) {
            result.issues.push_back({ GraphLoadCode::InvalidSchema, "Node '" + nodeId + "' has no valid position" });
            continue;
        }
        const double x = position->getProperty("x");
        const double y = position->getProperty("y");
        if (!std::isfinite(x) || !std::isfinite(y)) {
            result.issues.push_back({ GraphLoadCode::InvalidSchema, "Node '" + nodeId + "' position is not finite" });
            continue;
        }
        node.bounds.setPosition((float) x, (float) y);

        const auto* parameters = encoded->getProperty("parameters").getDynamicObject();
        if (parameters == nullptr) {
            result.issues.push_back({ GraphLoadCode::InvalidParameter, "Node '" + nodeId + "' parameters must be an object" });
            continue;
        }
        bool parametersValid = true;
        for (const auto& property : parameters->getProperties()) {
            const auto* parameterDefinition = registry.findParameter(node.kind, property.name.toString());
            String normalized;
            if (parameterDefinition == nullptr
                    || !parameterFromJSON(property.value, *parameterDefinition, normalized)) {
                result.issues.push_back({ GraphLoadCode::InvalidParameter,
                        "Invalid parameter '" + property.name.toString() + "' on node '" + nodeId + "'" });
                parametersValid = false;
                break;
            }
            auto found = std::find_if(node.parameters.begin(), node.parameters.end(), [&](const auto& parameter) {
                return parameter.id == property.name.toString();
            });
            jassert(found != node.parameters.end());
            found->value = parameterDefinition->normalized(normalized);
        }
        if (!parametersValid) {
            continue;
        }

        if (definition->modelCodec != nullptr) {
            String error;
            node.model = definition->modelCodec->readJSON(encoded->getProperty("model"), error);
            if (node.model == nullptr) {
                result.issues.push_back({ GraphLoadCode::InvalidModel,
                        "Invalid model for node '" + nodeId + "': " + error });
                continue;
            }
        } else if (!encoded->getProperty("model").isVoid()) {
            result.issues.push_back({ GraphLoadCode::InvalidModel, "Node '" + nodeId + "' does not accept a model" });
            continue;
        }
        const var editor = encoded->getProperty("editor");
        if (!editor.isVoid() && editor.getDynamicObject() == nullptr) {
            result.issues.push_back({ GraphLoadCode::InvalidSchema, "Node '" + nodeId + "' editor state must be an object" });
            continue;
        }
        node.editorState = editor;
        result.graph.addNode(std::move(node));
    }

    for (const auto& encodedValue : *encodedEdges) {
        const auto* encoded = encodedValue.getDynamicObject();
        Edge edge;
        if (encoded == nullptr
                || !readRequiredString(*encoded, "sourceNodeId", edge.sourceNodeId)
                || !readRequiredString(*encoded, "sourcePortId", edge.sourcePortId)
                || !readRequiredString(*encoded, "destNodeId", edge.destNodeId)
                || !readRequiredString(*encoded, "destPortId", edge.destPortId)) {
            result.issues.push_back({ GraphLoadCode::InvalidGraph, "Edge address fields must be non-empty strings" });
            continue;
        }
        const Node* sourceNode = result.graph.findNode(edge.sourceNodeId);
        const Node* destinationNode = result.graph.findNode(edge.destNodeId);
        const Port* source = sourceNode != nullptr ? findPort(*sourceNode, edge.sourcePortId, false) : nullptr;
        const Port* destination = destinationNode != nullptr ? findPort(*destinationNode, edge.destPortId, true) : nullptr;
        if (source == nullptr || destination == nullptr) {
            result.issues.push_back({ GraphLoadCode::InvalidGraph, "Edge references an unknown node or static port" });
            continue;
        }
        edge.domain = resolvedEdgeDomain(*source, *destination);
        edge.attachment = destination->purpose == PortPurpose::ScratchAttachment;
        result.graph.addEdge(std::move(edge));
    }

    std::unordered_set<String, StringHash> probeIds;
    for (const auto& encodedValue : *encodedProbes) {
        const auto* encoded = encodedValue.getDynamicObject();
        SignalProbe probe;
        if (encoded == nullptr
                || !readRequiredString(*encoded, "id", probe.id)
                || !readRequiredString(*encoded, "sourceNodeId", probe.sourceNodeId)
                || !readRequiredString(*encoded, "sourcePortId", probe.sourcePortId)
                || !readRequiredString(*encoded, "anchorDestNodeId", probe.anchorDestNodeId)
                || !readRequiredString(*encoded, "anchorDestPortId", probe.anchorDestPortId)
                || !probeIds.emplace(probe.id).second) {
            result.issues.push_back({ GraphLoadCode::DuplicateIdentity, "Invalid or duplicate signal probe identity" });
            continue;
        }
        probe.label = encoded->getProperty("label").toString();
        probe.tapPosition = (float) encoded->getProperty("tapPosition");
        probe.railOrder = (int) encoded->getProperty("railOrder");
        if (!std::isfinite(probe.tapPosition) || probe.tapPosition < 0.f || probe.tapPosition > 1.f) {
            result.issues.push_back({ GraphLoadCode::InvalidGraph, "Signal probe tap position must be finite and normalized" });
            continue;
        }
        result.graph.addSignalProbe(std::move(probe));
    }

    if (result.issues.empty()) {
        for (const auto& issue : GraphValidator().validate(result.graph)) {
            result.issues.push_back({ GraphLoadCode::InvalidGraph, issue.message });
        }
    }
    if (!result.issues.empty()) {
        result.graph = {};
    }
    return result;
}

String GraphSerializer::toJsonString(const NodeGraph& graph) const {
    String result;
    appendCanonicalJSON(writeJSON(graph), 0, result);
    return result + "\n";
}

NodeGraph GraphSerializer::fromJsonString(const String& json) const {
    return loadJsonString(json).graph;
}

GraphLoadResult GraphSerializer::loadJsonString(const String& json) const {
    if (json.trimStart().startsWithChar('<')) {
        GraphLoadResult result;
        result.issues.push_back({ GraphLoadCode::InvalidJson, "Legacy XML Cycle V2 graphs are not supported" });
        return result;
    }

    var parsed;
    const Result parseResult = JSON::parse(json, parsed);
    if (parseResult.failed()) {
        GraphLoadResult result;
        result.issues.push_back({ GraphLoadCode::InvalidJson, "Could not parse Cycle V2 graph JSON: " + parseResult.getErrorMessage() });
        return result;
    }
    return readJSON(parsed);
}

}

#include "Graph/NodeParameterMap.h"

namespace CycleV2 {

NodeParameterMap::NodeParameterMap(const std::vector<NodeParameter>& parameters) {
    values.reserve(parameters.size());
    for (const auto& parameter : parameters) {
        values.emplace(parameter.id, parameter.value);
    }
}

NodeParameterMap::NodeParameterMap(const Node& node) :
        NodeParameterMap(node.parameters) {}

bool NodeParameterMap::contains(const String& parameterId) const {
    return values.find(parameterId) != values.end();
}

const String* NodeParameterMap::find(const String& parameterId) const {
    const auto found = values.find(parameterId);
    return found != values.end() ? &found->second : nullptr;
}

String NodeParameterMap::stringValue(
        const String& parameterId,
        const String& fallback) const {
    const String* value = find(parameterId);
    return value != nullptr ? *value : fallback;
}

bool NodeParameterMap::boolValue(const String& parameterId, bool fallback) const {
    const String value = stringValue(parameterId, fallback ? "1" : "0").toLowerCase();
    return value == "1" || value == "true" || value == "on" || value == "yes";
}

int NodeParameterMap::intValue(const String& parameterId, int fallback) const {
    const String* value = find(parameterId);
    return value != nullptr && value->isNotEmpty() ? value->getIntValue() : fallback;
}

float NodeParameterMap::floatValue(const String& parameterId, float fallback) const {
    const String* value = find(parameterId);
    return value != nullptr && value->isNotEmpty() ? value->getFloatValue() : fallback;
}

}

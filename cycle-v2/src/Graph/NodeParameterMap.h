#pragma once

#include <unordered_map>

#include "NodeGraph.h"

namespace CycleV2 {

class NodeParameterMap {
public:
    explicit NodeParameterMap(const std::vector<NodeParameter>& parameters);
    explicit NodeParameterMap(const Node& node);

    bool contains(const String& parameterId) const;
    String stringValue(const String& parameterId, const String& fallback = {}) const;
    bool boolValue(const String& parameterId, bool fallback = false) const;
    int intValue(const String& parameterId, int fallback = 0) const;
    float floatValue(const String& parameterId, float fallback = 0.f) const;

private:
    struct StringHash {
        size_t operator()(const String& value) const {
            return (size_t) value.hashCode64();
        }
    };

    const String* find(const String& parameterId) const;
    std::unordered_map<String, String, StringHash> values;
};

}

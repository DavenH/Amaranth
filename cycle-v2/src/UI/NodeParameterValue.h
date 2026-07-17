#pragma once

#include "../Graph/NodeGraph.h"

namespace CycleV2 {

inline String nodeParameterValue(
        const Node& node,
        const String& parameterId,
        const String& fallback = {}) {
    for (const auto& parameter : node.parameters) {
        if (parameter.id == parameterId) {
            return parameter.value;
        }
    }

    return fallback;
}

}


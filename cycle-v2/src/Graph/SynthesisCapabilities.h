#pragma once

#include "Graph/NodeDefinition.h"

namespace CycleV2 {

// Read-only discovery; no document, editor or runtime is required.
class SynthesisCapabilities {
public:
    static var exportJSON(const ParameterMappingContext& context = {});
    static var convertParameter(
            const String& typeId,
            const String& parameterId,
            double value,
            bool toStored,
            const ParameterMappingContext& context,
            String& error);
};

}

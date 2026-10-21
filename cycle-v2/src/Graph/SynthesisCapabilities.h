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
    // Positions use the runtime's fixed-rate mapping; scaling includes the node gain/range.
    // Additive coefficients describe unipolar magnitudes, phaseScales describe bipolar phase values.
    static var spectralAuthoring(
            const std::vector<NodeParameter>& parameters,
            int midiNote,
            int cycleFrames,
            String& error);
};

}

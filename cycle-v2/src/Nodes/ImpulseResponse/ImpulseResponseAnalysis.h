#pragma once

#include <optional>
#include <vector>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct ImpulseResponseSource {
    std::vector<float> rawImpulse;
};

struct ImpulseResponseAnalysis {
    std::vector<float> filteredImpulse;
    std::vector<float> normalizedMagnitudes;
    std::vector<float> frequencyRows;
};

std::optional<ImpulseResponseSource> prepareImpulseResponseSource(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model,
        const AudioSampleResource* directResource = nullptr);

ImpulseResponseAnalysis prepareImpulseResponseAnalysis(
        const ImpulseResponseSource& source,
        float highPass);

std::optional<ImpulseResponseAnalysis> prepareImpulseResponseAnalysis(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model,
        const AudioSampleResource* directResource = nullptr);

}

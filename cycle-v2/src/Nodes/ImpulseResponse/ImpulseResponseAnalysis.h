#pragma once

#include <optional>
#include <vector>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

inline constexpr double impulseResponseReferenceSampleRate = 44100.0;
inline constexpr auto impulseResponseSampleRatePolicy =
        "Retain source samples; prepare at canonical 44100 Hz, then resample the filtered kernel to the execution rate";

bool isSupportedImpulseResponseSampleRate(double sampleRate);

struct ImpulseResponseSource {
    std::vector<float> rawImpulse;
    std::vector<float> displayImpulse;
};

struct ImpulseResponseAnalysis {
    std::vector<float> filteredImpulse;
    std::vector<float> filteredDisplayImpulse;
    std::vector<float> normalizedMagnitudes;
    std::vector<float> frequencyRows;
};

// Offline amplitude-preserving waveform conversion; callers own kernel gain policy.
std::vector<float> resampleImpulseResponse(
        const std::vector<float>& samples,
        double sourceSampleRate,
        double destinationSampleRate,
        int destinationSampleCount);

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

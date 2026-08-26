#pragma once

#include <Audio/CycleDsp/UnisonCore.h>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct UnisonPreviewContext {
    int midiNote { 60 };
    double voiceDurationSeconds { 1.0 };
    std::vector<float> pitchEnvelopeUnitValues;
};

struct UnisonPreviewPath {
    int voiceIndex {};
    float detuneCents {};
    float pan { 0.5f };
    std::vector<CycleDsp::UnisonPhaseSegment> segments;
};

juce::Colour unisonLaserColourForPan(float pan);
std::vector<UnisonPreviewPath> makeUnisonPreviewPaths(
        const Node& node,
        const UnisonPreviewContext& context = {});
void paintUnisonPhasePreview(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom,
        const UnisonPreviewContext& context = {});
bool paintEffectCompactPreview(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom);
void paintDelayPingPreview(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom);
void paintEqualizerResponsePreview(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        bool showDetails);
void paintEqualizerResponseData(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        const std::vector<float>& response,
        bool showDetails);
Point<float> equalizerBandControlPoint(
        Rectangle<float> area,
        const Node& node,
        int band);

}

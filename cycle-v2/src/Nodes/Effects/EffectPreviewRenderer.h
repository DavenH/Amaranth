#pragma once

#include "../../Graph/NodeGraph.h"

namespace CycleV2 {

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

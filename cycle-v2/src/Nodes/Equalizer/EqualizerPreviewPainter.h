#pragma once

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class EqualizerPreviewPainter {
public:
    void paint(
            Graphics& graphics,
            Rectangle<float> area,
            const Node& node,
            bool showDetails) const;
    void paintResponse(
            Graphics& graphics,
            Rectangle<float> area,
            const Node& node,
            const std::vector<float>& response,
            bool showDetails) const;
    Point<float> bandControlPoint(Rectangle<float> area, const Node& node, int band) const;
};

}

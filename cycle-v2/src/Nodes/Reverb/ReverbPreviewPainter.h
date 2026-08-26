#pragma once

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class ReverbPreviewPainter {
public:
    void paint(Graphics& graphics, Rectangle<float> area, const Node& node, float zoom) const;
};

}

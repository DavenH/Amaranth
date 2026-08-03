#pragma once

#include "../Graph/NodeGraph.h"

namespace CycleV2 {

struct NodePortGutters {
    float left {};
    float top {};
    float bottom {};
};

class NodePortLayout {
public:
    static NodePortGutters guttersFor(const Node& node, float zoom);
    static Rectangle<float> reservePortGutters(
            const Node& node,
            Rectangle<float> content,
            float zoom);
};

}

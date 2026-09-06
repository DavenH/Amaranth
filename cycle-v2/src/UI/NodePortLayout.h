#pragma once

#include "Graph/NodeGraph.h"

namespace CycleV2 {

enum class SinglePortLayout {
    LeftToRight,
    LeftToBottom,
    TopToBottom
};

bool supportsSinglePortLayout(const Node& node);
SinglePortLayout singlePortLayout(const Node& node);
SinglePortLayout nextSinglePortLayout(SinglePortLayout layout);
void applySinglePortLayout(Node& node, SinglePortLayout layout);

}

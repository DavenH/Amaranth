#pragma once

#include "Graph/NodeGraph.h"

namespace CycleV2 {

enum class SinglePortLayout {
    LeftToRight,
    LeftToBottom,
    TopToBottom
};

enum class OperationPortLayout {
    Side,
    Uptack,
    Vertical,
    Tee
};

bool supportsSinglePortLayout(const Node& node);
SinglePortLayout singlePortLayout(const Node& node);
SinglePortLayout nextSinglePortLayout(SinglePortLayout layout);
void applySinglePortLayout(Node& node, SinglePortLayout layout);

bool supportsOperationPortLayout(const Node& node);
OperationPortLayout operationPortLayout(const Node& node);
OperationPortLayout nextOperationPortLayout(OperationPortLayout layout);
void applyOperationPortLayout(Node& node, OperationPortLayout layout);

PortSide nextOutputPortSide(const Node& node);

}

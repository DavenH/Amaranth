#include "UI/NodePortLayout.h"

namespace CycleV2 {

bool supportsSinglePortLayout(const Node& node) {
    return node.inputs.size() == 1 && node.outputs.size() == 1;
}

SinglePortLayout singlePortLayout(const Node& node) {
    if (!supportsSinglePortLayout(node)) {
        return SinglePortLayout::LeftToRight;
    }

    const PortSide input = node.inputs.front().side;
    const PortSide output = node.outputs.front().side;
    if (input == PortSide::Left && output == PortSide::Bottom) {
        return SinglePortLayout::LeftToBottom;
    }
    if (input == PortSide::Top && output == PortSide::Bottom) {
        return SinglePortLayout::TopToBottom;
    }

    return SinglePortLayout::LeftToRight;
}

SinglePortLayout nextSinglePortLayout(SinglePortLayout layout) {
    switch (layout) {
        case SinglePortLayout::LeftToRight:  return SinglePortLayout::LeftToBottom;
        case SinglePortLayout::LeftToBottom: return SinglePortLayout::TopToBottom;
        case SinglePortLayout::TopToBottom:  return SinglePortLayout::LeftToRight;
    }

    return SinglePortLayout::LeftToRight;
}

void applySinglePortLayout(Node& node, SinglePortLayout layout) {
    if (!supportsSinglePortLayout(node)) {
        return;
    }

    switch (layout) {
        case SinglePortLayout::LeftToRight:
            node.inputs.front().side = PortSide::Left;
            node.outputs.front().side = PortSide::Right;
            break;
        case SinglePortLayout::LeftToBottom:
            node.inputs.front().side = PortSide::Left;
            node.outputs.front().side = PortSide::Bottom;
            break;
        case SinglePortLayout::TopToBottom:
            node.inputs.front().side = PortSide::Top;
            node.outputs.front().side = PortSide::Bottom;
            break;
    }
}

}

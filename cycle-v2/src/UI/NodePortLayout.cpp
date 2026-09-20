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

bool supportsOperationPortLayout(const Node& node) {
    return node.inputs.size() >= 2 && !node.outputs.empty();
}

OperationPortLayout operationPortLayout(const Node& node) {
    if (!supportsOperationPortLayout(node)) {
        return OperationPortLayout::Side;
    }

    const PortSide first = node.inputs[0].side;
    const PortSide second = node.inputs[1].side;
    if (first == PortSide::Left && second == PortSide::Bottom) {
        return OperationPortLayout::Tee;
    }
    if (first == PortSide::Top && second == PortSide::Bottom) {
        return OperationPortLayout::Vertical;
    }
    if (first == PortSide::Left && second == PortSide::Top) {
        return OperationPortLayout::Uptack;
    }

    return OperationPortLayout::Side;
}

OperationPortLayout nextOperationPortLayout(OperationPortLayout layout) {
    switch (layout) {
        case OperationPortLayout::Side:     return OperationPortLayout::Uptack;
        case OperationPortLayout::Uptack:   return OperationPortLayout::Vertical;
        case OperationPortLayout::Vertical: return OperationPortLayout::Tee;
        case OperationPortLayout::Tee:      return OperationPortLayout::Side;
    }

    return OperationPortLayout::Side;
}

void applyOperationPortLayout(Node& node, OperationPortLayout layout) {
    if (!supportsOperationPortLayout(node)) {
        return;
    }

    switch (layout) {
        case OperationPortLayout::Side:
            node.inputs[0].side = PortSide::Left;
            node.inputs[1].side = PortSide::Left;
            break;
        case OperationPortLayout::Uptack:
            node.inputs[0].side = PortSide::Left;
            node.inputs[1].side = PortSide::Top;
            break;
        case OperationPortLayout::Vertical:
            node.inputs[0].side = PortSide::Top;
            node.inputs[1].side = PortSide::Bottom;
            break;
        case OperationPortLayout::Tee:
            node.inputs[0].side = PortSide::Left;
            node.inputs[1].side = PortSide::Bottom;
            break;
    }
    node.outputs[0].side = PortSide::Right;
}

PortSide nextOutputPortSide(const Node& node) {
    const PortSide side = node.outputs.empty() ? PortSide::Right : node.outputs.front().side;
    switch (side) {
        case PortSide::Right:  return PortSide::Bottom;
        case PortSide::Bottom: return PortSide::Top;
        case PortSide::Top:    return PortSide::Right;
        case PortSide::Left:   return PortSide::Right;
    }

    return PortSide::Right;
}

}

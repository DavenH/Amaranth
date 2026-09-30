#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

enum class NodeCanvasCursorKind {
    Normal,
    VerticalAdjust,
    AddToSelection
};

class NodeCanvasCursorPolicy {
public:
    static NodeCanvasCursorKind cursorFor(
            bool hasVerticalAdjustment,
            ModifierKeys modifiers) {
        if (hasVerticalAdjustment) {
            return NodeCanvasCursorKind::VerticalAdjust;
        }
        return modifiers.isShiftDown()
                ? NodeCanvasCursorKind::AddToSelection
                : NodeCanvasCursorKind::Normal;
    }
};

}

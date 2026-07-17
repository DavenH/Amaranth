#pragma once

#include <JuceHeader.h>

#include <optional>

#include "../Graph/NodeGraph.h"

namespace CycleV2 {

enum class TransformMode {
    Cycle,
    FixedWindow,
    Cyclic,
    AcyclicCarry
};

class TransformCompactEditor {
public:
    static bool supports(NodeKind kind);
    static Rectangle<float> expandedContentBounds(Rectangle<float> panel);

    static TransformMode mode(const Node& node);
    static String parameterValue(TransformMode mode);
    static String subtitle(NodeKind kind, TransformMode mode);
    static String status(NodeKind kind, TransformMode mode);

    static void paint(Graphics& graphics, Rectangle<float> panel, const Node& node);
    static std::optional<TransformMode> modeAt(
            const Node& node,
            Rectangle<float> panel,
            Point<float> position);
};

}

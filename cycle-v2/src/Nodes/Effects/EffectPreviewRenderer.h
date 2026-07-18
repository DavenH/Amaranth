#pragma once

#include "../../Graph/NodeGraph.h"

namespace CycleV2 {

bool paintEffectCompactPreview(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom);

}

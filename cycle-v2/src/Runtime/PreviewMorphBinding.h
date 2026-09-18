#pragma once

#include <vector>

#include "Graph/GraphCompiler.h"
#include "Graph/PreviewMorphTarget.h"

namespace CycleV2 {

class PreviewMorphBinding final {
public:
    static std::vector<PreviewMorphTarget> fromPlan(const GraphExecutionPlan& plan);
};

}

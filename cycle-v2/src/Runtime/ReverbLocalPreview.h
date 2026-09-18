#pragma once

#include <memory>
#include <optional>

#include "Runtime/GraphPreviewExecutor.h"
#include "Nodes/Effects/EffectSignalProcessors.h"
#include "Graph/GraphCompiler.h"

namespace CycleV2 {

struct ReverbLocalPreviewInput {
    String nodeId;
    std::vector<NodeParameter> parameters;
    std::shared_ptr<const ReverbKernelData> previousKernel;
};

class ReverbLocalPreview final {
public:
    static std::optional<ReverbLocalPreviewInput> prepare(
            const Node& node,
            const GraphExecutionStep& step);
    static NodePreviewResult render(const ReverbLocalPreviewInput& input);
};

}

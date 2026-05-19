#pragma once

#include "NodeModuleRegistry.h"

#include <memory>
#include <vector>

namespace CycleV2 {

struct PreviewProcessContext {
    size_t pointCount {};
    std::vector<float> inputSummary;
    std::vector<float> primary;
    std::vector<float> secondary;
};

class NodePreviewProcessor {
public:
    virtual ~NodePreviewProcessor() = default;

    virtual PreviewModuleRole role() const = 0;
    virtual void render(PreviewProcessContext& context) = 0;
};

class NodePreviewProcessorFactory {
public:
    std::unique_ptr<NodePreviewProcessor> create(PreviewModuleRole role) const;
};

}

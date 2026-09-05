#pragma once

#include <cstdint>

namespace CycleV2 {

enum class NodeEditorPerformanceOperation : uint8_t {
    NodeParameterUpdate,
    NodeParameterCommit,
    TrimeshPointUpdate,
    TrimeshPointCommit,
    TrimeshVertexUpdate,
    TrimeshVertexCommit,
    TrimeshVertexSelection
};

class NodeEditorPerformanceObserver {
public:
    virtual ~NodeEditorPerformanceObserver() = default;
    virtual void nodeEditorOperationCompleted(
            NodeEditorPerformanceOperation operation,
            uint64_t elapsedMicroseconds) = 0;
};

}

#pragma once

#include <memory>

#include "Graph/GraphCompiler.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Runtime/GraphRuntime.h"

namespace CycleV2 {

class GraphPresentationFacts;

struct GraphPresentationSnapshot {
    uint64_t graphRevision {};
    int previewMidiNote { 48 };
    int previewModWheelValue {};
    GraphCompileResult compileResult;
    RuntimeProcessTrace runtimeTrace;
    GraphPreviewResult previewResult;
    std::shared_ptr<const GraphPresentationFacts> facts;
};

}

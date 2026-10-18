#pragma once

#include <array>
#include <memory>

#include "Graph/GraphCompiler.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Runtime/GraphRuntime.h"

namespace CycleV2 {

class GraphPresentationFacts;

struct GraphPresentationSnapshot {
    uint64_t graphRevision {};
    int previewMidiNote { 48 };
    std::array<int, 128> previewControllerValues {};
    GraphCompileResult compileResult;
    RuntimeProcessTrace runtimeTrace;
    GraphPreviewResult previewResult;
    std::shared_ptr<const GraphPresentationFacts> facts;
};

}

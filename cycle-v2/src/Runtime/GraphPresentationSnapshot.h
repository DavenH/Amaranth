#pragma once

#include "Graph/GraphCompiler.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Runtime/GraphRuntime.h"

namespace CycleV2 {

struct GraphPresentationSnapshot {
    uint64_t graphRevision {};
    int previewMidiNote { 48 };
    int previewModWheelValue {};
    GraphCompileResult compileResult;
    RuntimeProcessTrace runtimeTrace;
    GraphPreviewResult previewResult;
};

}

#pragma once

#include "Graph/GraphAudioScope.h"
#include "Graph/GraphCompiler.h"

namespace CycleV2 {

class GraphAudioScopeCompiler {
public:
    static void applyOwnership(
            GraphExecutionPlan& plan,
            const GraphAudioScopeAnalysis& analysis);
    static void compileVoiceMixBoundary(
            GraphExecutionPlan& plan,
            const GraphAudioScopeAnalysis& analysis);
};

}

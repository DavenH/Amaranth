#pragma once

#include <unordered_map>
#include <vector>

#include "Graph/NodeDefinition.h"

namespace CycleV2 {

enum class AuthoredAudioScope {
    Voice,
    Global
};

struct GraphAudioScopeAnalysis {
    struct StringHash {
        size_t operator()(const String& value) const {
            return static_cast<size_t>(value.hashCode64());
        }
    };

    std::unordered_map<String, AuthoredAudioScope, StringHash> nodes;
    std::vector<String> conflictingNeutralNodeIds;

    AuthoredAudioScope scopeFor(const String& nodeId) const;
    bool hasConflict(const String& nodeId) const;
};

class GraphAudioScopeAnalyzer {
public:
    GraphAudioScopeAnalysis analyze(const NodeGraph& graph) const;
    static AudioProcessingCapability capabilityFor(const Node& node);
    static AuthoredAudioScope explicitScopeFor(const Node& node);
};

}

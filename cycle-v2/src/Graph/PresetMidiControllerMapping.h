#pragma once

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "Graph/NodeGraph.h"
#include "Graph/NodeParameterMap.h"

namespace CycleV2 {

inline std::vector<int> mappedPresetMidiControllers(const NodeGraph& graph) {
    struct StringHash {
        size_t operator()(const String& value) const {
            return (size_t) value.hashCode64();
        }
    };
    std::vector<int> controllers;
    std::unordered_set<String, StringHash> attachedSources;
    for (const auto& edge : graph.getEdges()) {
        attachedSources.insert(edge.sourceNodeId);
    }
    for (const auto& node : graph.getNodes()) {
        if (node.kind != NodeKind::ModulationSource
                && node.kind != NodeKind::ModulationTriple) {
            continue;
        }
        if (attachedSources.find(node.id) == attachedSources.end()) {
            continue;
        }

        const NodeParameterMap parameters(node);
        const auto addSource = [&](const String& prefix) {
            const String source = parameters.stringValue(
                    prefix.isEmpty() ? "source" : prefix + "Source");
            if (source == "modWheel") {
                controllers.push_back(1);
            } else if (source == "midiCC") {
                controllers.push_back(juce::jlimit(0, 127,
                        parameters.intValue(prefix.isEmpty()
                                ? "controller" : prefix + "Controller", 1)));
            }
        };
        if (node.kind == NodeKind::ModulationSource) {
            addSource({});
        } else {
            for (const String& prefix : { String("yellow"), String("red"), String("blue") }) {
                addSource(prefix);
            }
        }
    }
    std::sort(controllers.begin(), controllers.end());
    controllers.erase(std::unique(controllers.begin(), controllers.end()),
            controllers.end());
    return controllers;
}

}

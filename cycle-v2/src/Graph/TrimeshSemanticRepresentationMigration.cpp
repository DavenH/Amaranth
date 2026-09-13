#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "Graph/TrimeshSemanticRepresentationMigration.h"

#include "Graph/NodeGraph.h"

namespace CycleV2 {

namespace {

struct StringHash {
    size_t operator()(const String& value) const {
        return static_cast<size_t>(value.hashCode64());
    }
};

using NodeMap = std::unordered_map<String, DynamicObject*, StringHash>;

bool isSignalEdge(const DynamicObject& edge) {
    const String kind = edge.getProperty("connectionKind").toString();
    return kind.isEmpty() || kind == "signal";
}

String legacyVoiceDomain(
        const NodeMap& nodes,
        const Array<var>& edges,
        const String& meshId) {
    for (const auto& encoded : edges) {
        const auto* edge = encoded.getDynamicObject();
        if (edge == nullptr
                || edge->getProperty("destNodeId").toString() != meshId
                || edge->getProperty("destPortId").toString() != "context") {
            continue;
        }
        const auto source = nodes.find(edge->getProperty("sourceNodeId").toString());
        if (source == nodes.end()
                || source->second->getProperty("kind").toString() != "voiceContext") {
            continue;
        }
        const auto* parameters = source->second->getProperty("parameters").getDynamicObject();
        return parameters != nullptr
                ? parameters->getProperty("domain").toString()
                : String("waveform");
    }
    return {};
}

String downstreamSpectralType(
        const NodeMap& nodes,
        const Array<var>& edges,
        const String& meshId) {
    std::deque<String> pending { meshId };
    std::unordered_set<String, StringHash> visited { meshId };
    while (!pending.empty()) {
        const String sourceId = pending.front();
        pending.pop_front();
        for (const auto& encoded : edges) {
            const auto* edge = encoded.getDynamicObject();
            if (edge == nullptr || !isSignalEdge(*edge)
                    || edge->getProperty("sourceNodeId").toString() != sourceId) {
                continue;
            }
            const String destinationId = edge->getProperty("destNodeId").toString();
            const auto destination = nodes.find(destinationId);
            if (destination == nodes.end()) {
                continue;
            }
            const String destinationKind = destination->second->getProperty("kind").toString();
            const String destinationPort = edge->getProperty("destPortId").toString();
            if (destinationKind == "ifft") {
                return destinationPort == "phase"
                        ? String("spectralPhase")
                        : String("spectralMagnitude");
            }
            if ((destinationKind == "add"
                        || destinationKind == "multiply"
                        || destinationKind == "spectralLayer")
                    && visited.emplace(destinationId).second) {
                pending.push_back(destinationId);
            }
        }
    }
    return "spectralMagnitude";
}

bool feedsMultiply(
        const NodeMap& nodes,
        const Array<var>& edges,
        const String& meshId) {
    String sourceId = meshId;
    for (int depth = 0; depth < 2; ++depth) {
        bool followedPan {};
        for (const auto& encoded : edges) {
            const auto* edge = encoded.getDynamicObject();
            if (edge == nullptr || !isSignalEdge(*edge)
                    || edge->getProperty("sourceNodeId").toString() != sourceId) {
                continue;
            }
            const String destinationId = edge->getProperty("destNodeId").toString();
            const auto destination = nodes.find(destinationId);
            if (destination == nodes.end()) {
                return false;
            }
            const String kind = destination->second->getProperty("kind").toString();
            if (kind == "multiply") {
                return true;
            }
            if (kind == "spectralLayer") {
                sourceId = destinationId;
                followedPan = true;
                break;
            }
            return false;
        }
        if (!followedPan) {
            return false;
        }
    }
    return false;
}

bool isImplicitContextDestination(const String& kind) {
    return kind == "waveSource"
            || kind == "imageSource"
            || kind == "trilinearMesh";
}

}

TrimeshSemanticRepresentationMigrationResult
TrimeshSemanticRepresentationMigration::migrate(var& graph) const {
    TrimeshSemanticRepresentationMigrationResult result;
    var candidate = graph.clone();
    auto* root = candidate.getDynamicObject();
    if (root == nullptr || root->getProperty("format").toString() != "cycle-v2-graph") {
        result.error = "Root object is not a Cycle V2 graph";
        return result;
    }
    const int version = (int) root->getProperty("formatVersion");
    if (version == 7) {
        return result;
    }
    if (version != 6) {
        result.error = "Unsupported Cycle V2 graph format version";
        return result;
    }
    auto* encodedNodes = root->getProperty("nodes").getArray();
    auto* edges = root->getProperty("edges").getArray();
    if (encodedNodes == nullptr || edges == nullptr) {
        result.error = "Graph nodes and edges must be arrays";
        return result;
    }

    NodeMap nodes;
    int voiceContextCount {};
    for (auto& encoded : *encodedNodes) {
        auto* node = encoded.getDynamicObject();
        if (node == nullptr) {
            result.error = "Graph nodes must be objects";
            return result;
        }
        nodes.emplace(node->getProperty("id").toString(), node);
        voiceContextCount += node->getProperty("kind").toString() == "voiceContext" ? 1 : 0;
    }

    for (auto& encoded : *encodedNodes) {
        auto* node = encoded.getDynamicObject();
        auto* parameters = node->getProperty("parameters").getDynamicObject();
        if (parameters == nullptr) {
            result.error = "Node parameters must be objects";
            return result;
        }
        const String kind = node->getProperty("kind").toString();
        if (kind == "spectralLayer") {
            parameters->removeProperty("mode");
        } else if (kind == "trilinearMesh") {
            const String nodeId = node->getProperty("id").toString();
            if (parameters->hasProperty("signalType")
                    && parameters->hasProperty("polarity")) {
                parameters->removeProperty("spectralMode");
                continue;
            }
            const String legacyDomain = legacyVoiceDomain(nodes, *edges, nodeId);
            const String signalType = legacyDomain == "spectralPhase"
                    ? String("spectralPhase")
                    : (legacyDomain.isEmpty()
                                    || legacyDomain == "spectral"
                                    || legacyDomain == "spectralMagnitude"
                            ? downstreamSpectralType(nodes, *edges, nodeId)
                            : String("time"));
            const String legacyMode = parameters->getProperty("spectralMode").toString();
            const bool bipolar = signalType != "spectralMagnitude"
                    || legacyMode == "multiplicative"
                    || (legacyMode != "additive" && feedsMultiply(nodes, *edges, nodeId));
            parameters->removeProperty("spectralMode");
            parameters->setProperty("signalType", signalType);
            parameters->setProperty("polarity", bipolar ? "bipolar" : "unipolar");
        }
    }

    for (auto& encoded : *encodedNodes) {
        auto* node = encoded.getDynamicObject();
        if (node->getProperty("kind").toString() == "voiceContext") {
            node->getProperty("parameters").getDynamicObject()
                    ->removeProperty("domain");
        }
    }

    if (voiceContextCount == 1) {
        for (int index = edges->size(); --index >= 0;) {
            const auto* edge = edges->getReference(index).getDynamicObject();
            if (edge == nullptr
                    || edge->getProperty("destPortId").toString() != "context") {
                continue;
            }
            const auto source = nodes.find(edge->getProperty("sourceNodeId").toString());
            const auto destination = nodes.find(edge->getProperty("destNodeId").toString());
            if (source != nodes.end() && destination != nodes.end()
                    && source->second->getProperty("kind").toString() == "voiceContext"
                    && isImplicitContextDestination(
                            destination->second->getProperty("kind").toString())) {
                edges->remove(index);
            }
        }
    }

    root->setProperty("formatVersion", 7);
    graph = std::move(candidate);
    result.migrated = true;
    return result;
}

}

#include "Graph/GlobalAudioGraphRepresentationMigration.h"

#include "Graph/GlobalAudioGraphMigration.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/NodeDefinition.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace CycleV2 {

namespace {

struct StringHash {
    size_t operator()(const String& value) const {
        return static_cast<size_t>(value.hashCode64());
    }
};

using StringSet = std::unordered_set<String, StringHash>;

bool isLegacyGlobalKind(const String& kind) {
    return kind == "waveshaper"
            || kind == "impulseResponse"
            || kind == "equalizer"
            || kind == "delay"
            || kind == "reverb"
            || kind == "output";
}

bool isSelectableKind(const String& kind) {
    return kind == "waveshaper"
            || kind == "impulseResponse"
            || kind == "equalizer";
}

bool isSignalEdge(const DynamicObject& edge) {
    const String kind = edge.getProperty("connectionKind").toString();
    return kind.isEmpty() || kind == "signal";
}

Rectangle<float> nodeBounds(const DynamicObject& encoded) {
    const auto* position = encoded.getProperty("position").getDynamicObject();
    const auto* definition = NodeDefinitionRegistry::instance().find(
            encoded.getProperty("kind").toString());
    if (position == nullptr || definition == nullptr) {
        return {};
    }
    const Node node = GraphNodeFactory().createNode(definition->kind, "layout", {});
    return {
            (float) position->getProperty("x"),
            (float) position->getProperty("y"),
            node.bounds.getWidth(),
            node.bounds.getHeight()
    };
}

void setPosition(DynamicObject& node, Point<float> position) {
    auto encoded = std::make_unique<DynamicObject>();
    encoded->setProperty("x", position.x);
    encoded->setProperty("y", position.y);
    node.setProperty("position", var(encoded.release()));
}

DynamicObject* nodeWithId(Array<var>& nodes, const String& nodeId) {
    for (auto& encoded : nodes) {
        auto* node = encoded.getDynamicObject();
        if (node != nullptr && node->getProperty("id").toString() == nodeId) {
            return node;
        }
    }
    return nullptr;
}

const DynamicObject* nodeWithId(const Array<var>& nodes, const String& nodeId) {
    for (const auto& encoded : nodes) {
        const auto* node = encoded.getDynamicObject();
        if (node != nullptr && node->getProperty("id").toString() == nodeId) {
            return node;
        }
    }
    return nullptr;
}

String kindForNode(const Array<var>& nodes, const String& nodeId) {
    for (const auto& encoded : nodes) {
        const auto* node = encoded.getDynamicObject();
        if (node != nullptr && node->getProperty("id").toString() == nodeId) {
            return node->getProperty("kind").toString();
        }
    }
    return {};
}

String relocatePreEnvelopeEffectChain(Array<var>& nodes, Array<var>& edges) {
    for (int boundaryIndex = 0; boundaryIndex < edges.size(); ++boundaryIndex) {
        auto* boundary = edges.getReference(boundaryIndex).getDynamicObject();
        if (boundary == nullptr || !isSignalEdge(*boundary)) {
            continue;
        }
        const String firstEffect = boundary->getProperty("destNodeId").toString();
        if (!isSelectableKind(kindForNode(nodes, firstEffect))
                && kindForNode(nodes, firstEffect) != "delay"
                && kindForNode(nodes, firstEffect) != "reverb") {
            continue;
        }

        String current = firstEffect;
        int effectOutputIndex = -1;
        while (true) {
            effectOutputIndex = -1;
            for (int index = 0; index < edges.size(); ++index) {
                const auto* edge = edges.getReference(index).getDynamicObject();
                if (edge != nullptr && isSignalEdge(*edge)
                        && edge->getProperty("sourceNodeId").toString() == current) {
                    effectOutputIndex = index;
                    break;
                }
            }
            if (effectOutputIndex < 0) {
                break;
            }
            const auto* output = edges.getReference(effectOutputIndex).getDynamicObject();
            const String destination = output->getProperty("destNodeId").toString();
            if (isLegacyGlobalKind(kindForNode(nodes, destination))
                    && kindForNode(nodes, destination) != "output") {
                current = destination;
                continue;
            }
            if (kindForNode(nodes, destination) != "multiply") {
                break;
            }

            int combinerOutputIndex = -1;
            bool hasVoiceSideInput {};
            for (int index = 0; index < edges.size(); ++index) {
                const auto* edge = edges.getReference(index).getDynamicObject();
                if (edge == nullptr || !isSignalEdge(*edge)) {
                    continue;
                }
                if (edge->getProperty("sourceNodeId").toString() == destination
                        && kindForNode(
                                nodes,
                                edge->getProperty("destNodeId").toString()) == "output") {
                    combinerOutputIndex = index;
                }
                if (edge->getProperty("destNodeId").toString() == destination
                        && edge->getProperty("sourceNodeId").toString() != current) {
                    hasVoiceSideInput = true;
                }
            }
            if (!hasVoiceSideInput || combinerOutputIndex < 0) {
                break;
            }

            auto* effectOutput = edges.getReference(effectOutputIndex).getDynamicObject();
            const auto* combinerOutput = edges.getReference(combinerOutputIndex)
                    .getDynamicObject();
            boundary->setProperty("destNodeId", destination);
            boundary->setProperty(
                    "destPortId",
                    effectOutput->getProperty("destPortId"));
            effectOutput->setProperty(
                    "destNodeId",
                    combinerOutput->getProperty("destNodeId"));
            effectOutput->setProperty(
                    "destPortId",
                    combinerOutput->getProperty("destPortId"));
            edges.remove(combinerOutputIndex);
            return firstEffect;
        }
    }
    return {};
}

bool isLegacyBoundaryDestination(
        const Array<var>& nodes,
        const DynamicObject& edge) {
    for (const auto& encoded : nodes) {
        const auto* node = encoded.getDynamicObject();
        if (node != nullptr
                && node->getProperty("id").toString()
                        == edge.getProperty("destNodeId").toString()) {
            return isLegacyGlobalKind(node->getProperty("kind").toString());
        }
    }
    return false;
}

StringSet classifyGlobalNodes(const Array<var>& nodes, const Array<var>& edges) {
    StringSet result;
    for (const auto& encoded : nodes) {
        const auto* node = encoded.getDynamicObject();
        if (node != nullptr && isLegacyGlobalKind(node->getProperty("kind").toString())) {
            result.emplace(node->getProperty("id").toString());
        }
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& encoded : edges) {
            const auto* edge = encoded.getDynamicObject();
            if (edge == nullptr || !isSignalEdge(*edge)
                    || result.find(edge->getProperty("sourceNodeId").toString())
                            == result.end()) {
                continue;
            }
            changed = result.emplace(
                    edge->getProperty("destNodeId").toString()).second || changed;
        }
    }
    return result;
}

std::vector<String> topologicalOrder(
        const Array<var>& nodes,
        const Array<var>& edges,
        const StringSet& globalIds) {
    std::unordered_map<String, int, StringHash> indegree;
    for (const auto& encoded : nodes) {
        const auto* node = encoded.getDynamicObject();
        const String id = node != nullptr ? node->getProperty("id").toString() : String();
        if (globalIds.find(id) != globalIds.end()) {
            indegree.emplace(id, 0);
        }
    }
    for (const auto& encoded : edges) {
        const auto* edge = encoded.getDynamicObject();
        if (edge == nullptr || !isSignalEdge(*edge)) {
            continue;
        }
        const String source = edge->getProperty("sourceNodeId").toString();
        const String destination = edge->getProperty("destNodeId").toString();
        if (globalIds.find(source) != globalIds.end()
                && globalIds.find(destination) != globalIds.end()) {
            ++indegree[destination];
        }
    }

    std::vector<String> result;
    while (result.size() < indegree.size()) {
        bool progressed {};
        for (const auto& encoded : nodes) {
            const auto* node = encoded.getDynamicObject();
            const String id = node != nullptr ? node->getProperty("id").toString() : String();
            const auto found = indegree.find(id);
            if (found == indegree.end() || found->second != 0
                    || std::find(result.begin(), result.end(), id) != result.end()) {
                continue;
            }
            result.push_back(id);
            found->second = -1;
            for (const auto& candidate : edges) {
                const auto* edge = candidate.getDynamicObject();
                if (edge == nullptr || !isSignalEdge(*edge)
                        || edge->getProperty("sourceNodeId").toString() != id) {
                    continue;
                }
                const auto destination = indegree.find(
                        edge->getProperty("destNodeId").toString());
                if (destination != indegree.end() && destination->second > 0) {
                    --destination->second;
                }
            }
            progressed = true;
        }
        if (!progressed) {
            return {};
        }
    }
    return result;
}

bool hasLinearGlobalTopology(
        const Array<var>& edges,
        const StringSet& globalIds) {
    std::unordered_map<String, int, StringHash> inputCounts;
    std::unordered_map<String, int, StringHash> outputCounts;
    for (const auto& encoded : edges) {
        const auto* edge = encoded.getDynamicObject();
        if (edge == nullptr || !isSignalEdge(*edge)) {
            continue;
        }
        const String source = edge->getProperty("sourceNodeId").toString();
        const String destination = edge->getProperty("destNodeId").toString();
        if (globalIds.find(source) == globalIds.end()
                || globalIds.find(destination) == globalIds.end()) {
            continue;
        }
        if (++outputCounts[source] > 1 || ++inputCounts[destination] > 1) {
            return false;
        }
    }
    return true;
}

void removePortSideOverride(
        DynamicObject& node,
        const Identifier& group,
        const String& portId) {
    auto* sides = node.getProperty("portSides").getDynamicObject();
    auto* ports = sides != nullptr ? sides->getProperty(group).getDynamicObject() : nullptr;
    if (ports == nullptr) {
        return;
    }
    ports->removeProperty(portId);
    if (ports->getProperties().size() == 0) {
        sides->removeProperty(group);
    }
    if (sides->getProperties().size() == 0) {
        node.removeProperty("portSides");
    }
}

void normalizeLinearGlobalPortSides(
        Array<var>& nodes,
        const Array<var>& edges,
        const StringSet& globalIds) {
    if (!hasLinearGlobalTopology(edges, globalIds)) {
        return;
    }
    for (const auto& encoded : edges) {
        const auto* edge = encoded.getDynamicObject();
        if (edge == nullptr || !isSignalEdge(*edge)) {
            continue;
        }
        const String sourceId = edge->getProperty("sourceNodeId").toString();
        const String destinationId = edge->getProperty("destNodeId").toString();
        if (globalIds.find(sourceId) == globalIds.end()
                || globalIds.find(destinationId) == globalIds.end()) {
            continue;
        }
        if (auto* source = nodeWithId(nodes, sourceId)) {
            removePortSideOverride(
                    *source,
                    "outputs",
                    edge->getProperty("sourcePortId").toString());
        }
        if (auto* destination = nodeWithId(nodes, destinationId)) {
            removePortSideOverride(
                    *destination,
                    "inputs",
                    edge->getProperty("destPortId").toString());
        }
    }
}

void applyGlobalLayout(
        Array<var>& nodes,
        const Array<var>& edges,
        const StringSet& globalIds,
        const std::vector<String>& order) {
    Rectangle<float> voice;
    bool hasVoiceBounds {};
    for (const auto& encoded : nodes) {
        const auto* node = encoded.getDynamicObject();
        if (node == nullptr
                || globalIds.find(node->getProperty("id").toString()) != globalIds.end()) {
            continue;
        }
        const Rectangle<float> bounds = nodeBounds(*node);
        voice = hasVoiceBounds ? voice.getUnion(bounds) : bounds;
        hasVoiceBounds = true;
    }

    const float startX = hasVoiceBounds ? voice.getX() : 100.f;
    float x = startX;
    float y = hasVoiceBounds
            ? voice.getBottom() + GlobalAudioGraphMigration::laneGap
            : 100.f;
    float rowHeight {};
    for (const auto& nodeId : order) {
        DynamicObject* node = nodeWithId(nodes, nodeId);
        if (node == nullptr) {
            continue;
        }
        const Rectangle<float> bounds = nodeBounds(*node);
        if (x > startX && x + bounds.getWidth()
                > startX + GlobalAudioGraphMigration::maximumRowWidth) {
            x = startX;
            y += rowHeight + GlobalAudioGraphMigration::nodeClearance;
            rowHeight = 0.f;
        }
        setPosition(*node, { x, y });
        x += bounds.getWidth() + GlobalAudioGraphMigration::nodeClearance;
        rowHeight = jmax(rowHeight, bounds.getHeight());
    }
    normalizeLinearGlobalPortSides(nodes, edges, globalIds);
}

var globalInputNode() {
    auto node = std::make_unique<DynamicObject>();
    node->setProperty("id", "globalInput");
    node->setProperty("kind", "globalInput");
    node->setProperty("definitionVersion", 1);
    setPosition(*node, {});
    node->setProperty("parameters", var(new DynamicObject()));
    return var(node.release());
}

var globalInputEdge(const String& destinationNodeId, const String& destinationPortId) {
    auto edge = std::make_unique<DynamicObject>();
    edge->setProperty("sourceNodeId", "globalInput");
    edge->setProperty("sourcePortId", "time");
    edge->setProperty("destNodeId", destinationNodeId);
    edge->setProperty("destPortId", destinationPortId);
    edge->setProperty("connectionKind", "signal");
    edge->setProperty("attachmentType", "none");
    return var(edge.release());
}

var voiceOutputNode(Point<float> position) {
    auto node = std::make_unique<DynamicObject>();
    node->setProperty("id", "voiceOutput");
    node->setProperty("kind", "voiceOutput");
    node->setProperty("definitionVersion", 1);
    setPosition(*node, position);
    node->setProperty("parameters", var(new DynamicObject()));
    return var(node.release());
}

var voiceOutputEdge(const String& sourceNodeId, const String& sourcePortId) {
    auto edge = std::make_unique<DynamicObject>();
    edge->setProperty("sourceNodeId", sourceNodeId);
    edge->setProperty("sourcePortId", sourcePortId);
    edge->setProperty("destNodeId", "voiceOutput");
    edge->setProperty("destPortId", "time");
    edge->setProperty("connectionKind", "signal");
    edge->setProperty("attachmentType", "none");
    return var(edge.release());
}

bool outputCarriesLinkedStereoTime(
        const Array<var>& nodes,
        const Array<var>& edges,
        const String& nodeId,
        const String& portId,
        StringSet& visited) {
    if (!visited.emplace(nodeId).second) {
        return false;
    }
    const auto* node = nodeWithId(nodes, nodeId);
    const auto* definition = node != nullptr
            ? NodeDefinitionRegistry::instance().find(node->getProperty("kind").toString())
            : nullptr;
    if (definition == nullptr) {
        return false;
    }
    const auto output = std::find_if(
            definition->outputs.begin(),
            definition->outputs.end(),
            [&](const Port& candidate) { return candidate.id == portId; });
    if (output != definition->outputs.end()
            && output->domain == PortDomain::TimeSignal
            && output->channelLayout == ChannelLayout::LinkedStereo) {
        return true;
    }
    if (definition->processingCapability != AudioProcessingCapability::DomainNeutral) {
        return false;
    }
    return std::any_of(edges.begin(), edges.end(), [&](const var& value) {
        const auto* edge = value.getDynamicObject();
        if (edge == nullptr || !isSignalEdge(*edge)
                || edge->getProperty("destNodeId").toString() != nodeId) {
            return false;
        }
        StringSet branchVisited = visited;
        return outputCarriesLinkedStereoTime(
                nodes,
                edges,
                edge->getProperty("sourceNodeId").toString(),
                edge->getProperty("sourcePortId").toString(),
                branchVisited);
    });
}

std::vector<std::pair<String, String>> voiceTerminals(
        const Array<var>& nodes,
        const Array<var>& edges) {
    std::vector<std::pair<String, String>> terminals;
    for (const auto& encoded : nodes) {
        const auto* node = encoded.getDynamicObject();
        if (node == nullptr) {
            continue;
        }
        const String nodeId = node->getProperty("id").toString();
        const auto* definition = NodeDefinitionRegistry::instance().find(
                node->getProperty("kind").toString());
        if (definition == nullptr
                || definition->processingCapability == AudioProcessingCapability::GlobalOnly) {
            continue;
        }
        for (const auto& output : definition->outputs) {
            StringSet visited;
            if (!outputCarriesLinkedStereoTime(
                    nodes,
                    edges,
                    nodeId,
                    output.id,
                    visited)) {
                continue;
            }
            const bool consumed = std::any_of(edges.begin(), edges.end(), [&](const var& value) {
                const auto* edge = value.getDynamicObject();
                return edge != nullptr && isSignalEdge(*edge)
                        && edge->getProperty("sourceNodeId").toString() == nodeId
                        && edge->getProperty("sourcePortId").toString() == output.id;
            });
            if (!consumed) {
                terminals.push_back({ nodeId, output.id });
            }
        }
    }
    return terminals;
}

}

GlobalAudioGraphRepresentationMigrationResult
GlobalAudioGraphRepresentationMigration::migrate(var& graph) const {
    GlobalAudioGraphRepresentationMigrationResult result;
    var candidate = graph.clone();
    auto* root = candidate.getDynamicObject();
    if (root == nullptr || root->getProperty("format").toString() != "cycle-v2-graph") {
        result.error = "Root object is not a Cycle V2 graph";
        return result;
    }
    const int version = (int) root->getProperty("formatVersion");
    if (version == 6) {
        return result;
    }
    if (version == 5) {
        auto* nodes = root->getProperty("nodes").getArray();
        auto* edges = root->getProperty("edges").getArray();
        if (nodes == nullptr || edges == nullptr) {
            result.error = "Graph nodes and edges must be arrays";
            return result;
        }
        if (nodeWithId(*nodes, "voiceOutput") != nullptr) {
            result.error = "Format 5 graph already uses the reserved 'voiceOutput' identity";
            return result;
        }
        const auto terminals = voiceTerminals(*nodes, *edges);
        if (terminals.size() > 1) {
            result.error = "Graph has multiple possible voice terminals";
            return result;
        }
        const auto terminal = terminals.empty()
                ? std::pair<String, String> {}
                : terminals.front();
        const auto* source = nodeWithId(*nodes, terminal.first);
        const Rectangle<float> bounds = source != nullptr ? nodeBounds(*source) : Rectangle<float>();
        nodes->add(voiceOutputNode({
                bounds.getRight() + GlobalAudioGraphMigration::nodeClearance,
                bounds.getY()
        }));
        if (terminal.first.isNotEmpty()) {
            edges->add(voiceOutputEdge(terminal.first, terminal.second));
        }
        root->setProperty("formatVersion", 6);
        graph = std::move(candidate);
        result.migrated = true;
        return result;
    }
    if (version != 4) {
        result.error = "Unsupported Cycle V2 graph format version";
        return result;
    }
    auto* nodes = root->getProperty("nodes").getArray();
    auto* edges = root->getProperty("edges").getArray();
    if (nodes == nullptr || edges == nullptr) {
        result.error = "Graph nodes and edges must be arrays";
        return result;
    }
    if (nodeWithId(*nodes, "globalInput") != nullptr) {
        result.error = "Legacy graph already uses the reserved 'globalInput' identity";
        return result;
    }

    const String relocatedDestination = relocatePreEnvelopeEffectChain(*nodes, *edges);
    StringSet globalIds = classifyGlobalNodes(*nodes, *edges);
    std::vector<int> boundaries;
    for (int index = 0; index < edges->size(); ++index) {
        const auto* edge = edges->getReference(index).getDynamicObject();
        if (edge != nullptr && isSignalEdge(*edge)
                && isLegacyBoundaryDestination(*nodes, *edge)
                && globalIds.find(edge->getProperty("sourceNodeId").toString())
                        == globalIds.end()
                && globalIds.find(edge->getProperty("destNodeId").toString())
                        != globalIds.end()) {
            boundaries.push_back(index);
        }
    }
    if (boundaries.size() > 1) {
        result.error = "Legacy graph has multiple voice/global audio boundaries";
        return result;
    }

    String destinationNodeId = relocatedDestination;
    String destinationPortId { "time" };
    if (destinationNodeId.isEmpty() && !boundaries.empty()) {
        const auto* boundary = edges->getReference(boundaries.front()).getDynamicObject();
        destinationNodeId = boundary->getProperty("destNodeId").toString();
        destinationPortId = boundary->getProperty("destPortId").toString();
        edges->remove(boundaries.front());
    } else if (destinationNodeId.isEmpty()) {
        for (const auto& encoded : *nodes) {
            const auto* node = encoded.getDynamicObject();
            if (node != nullptr && node->getProperty("kind").toString() == "output") {
                destinationNodeId = node->getProperty("id").toString();
                break;
            }
        }
        if (destinationNodeId.isEmpty()) {
            result.error = "Legacy graph has no Output node";
            return result;
        }
    }

    for (auto& encoded : *nodes) {
        auto* node = encoded.getDynamicObject();
        if (node == nullptr || !isSelectableKind(node->getProperty("kind").toString())) {
            continue;
        }
        auto* parameters = node->getProperty("parameters").getDynamicObject();
        if (parameters == nullptr) {
            result.error = "Selectable effect parameters must be an object";
            return result;
        }
        parameters->setProperty("processingScope", "global");
    }
    nodes->add(globalInputNode());
    edges->add(globalInputEdge(destinationNodeId, destinationPortId));
    globalIds.emplace("globalInput");
    const std::vector<String> order = topologicalOrder(*nodes, *edges, globalIds);
    if (order.size() != globalIds.size()) {
        result.error = "Legacy global audio graph is cyclic or disconnected";
        return result;
    }
    applyGlobalLayout(*nodes, *edges, globalIds, order);
    root->setProperty("formatVersion", 5);
    graph = std::move(candidate);
    auto voiceMigration = migrate(graph);
    voiceMigration.globalNodeIds = order;
    return voiceMigration;
}

}

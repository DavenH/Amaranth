#include "Graph/OutputStageRepresentationMigration.h"

#include "Graph/DefaultOutputProbeResolver.h"
#include "Graph/NodeDefinition.h"

namespace CycleV2 {

namespace {

juce::DynamicObject* nodeWithId(juce::Array<juce::var>& nodes, const juce::String& id) {
    for (auto& encoded : nodes) {
        auto* node = encoded.getDynamicObject();
        if (node != nullptr && node->getProperty("id").toString() == id) {
            return node;
        }
    }
    return nullptr;
}

void shiftRight(juce::DynamicObject& node, float distance) {
    auto* position = node.getProperty("position").getDynamicObject();
    if (position != nullptr) {
        position->setProperty("x", (double) position->getProperty("x") + distance);
    }
}

}

juce::Result OutputStageRepresentationMigration::migrate(juce::var& graph) const {
    auto* root = graph.getDynamicObject();
    if (root == nullptr || (int) root->getProperty("formatVersion") != 9) {
        return juce::Result::ok();
    }
    auto* nodes = root->getProperty("nodes").getArray();
    auto* edges = root->getProperty("edges").getArray();
    if (nodes == nullptr || edges == nullptr) {
        return juce::Result::fail("Output migration requires node and edge arrays");
    }
    NodeGraph topology;
    juce::DynamicObject* legacy {};
    for (auto& encoded : *nodes) {
        auto* node = encoded.getDynamicObject();
        if (node == nullptr) {
            return juce::Result::fail("Output migration requires node objects");
        }
        Node description;
        description.id = node->getProperty("id").toString();
        description.kind = node->getProperty("kind").toString() == "output"
                ? NodeKind::GlobalOutput : NodeDefinitionRegistry::instance().kindForTypeId(node->getProperty("kind").toString());
        topology.addNode(std::move(description));
        if (node->getProperty("kind").toString() == "output") {
            if (legacy != nullptr) {
                return juce::Result::fail("Output migration requires one legacy output");
            }
            legacy = node;
        }
    }
    if (legacy == nullptr) {
        root->setProperty("formatVersion", 10);
        return juce::Result::ok();
    }
    for (const auto& encoded : *edges) {
        const auto* edge = encoded.getDynamicObject();
        if (edge == nullptr) {
            return juce::Result::fail("Output migration requires edge objects");
        }
        const auto kind = edge->getProperty("connectionKind").toString();
        if (!kind.isEmpty() && kind != "signal") {
            continue;
        }
        topology.addEdge({ edge->getProperty("sourceNodeId").toString(),
                edge->getProperty("sourcePortId").toString(),
                edge->getProperty("destNodeId").toString(),
                edge->getProperty("destPortId").toString(), PortDomain::TimeSignal,
                ConnectionKind::Signal });
    }
    const auto address = DefaultOutputProbeResolver().resolve(topology);
    if (!address.has_value()) {
        return juce::Result::fail("Legacy output has no unambiguous signal path for Gain migration");
    }
    const auto gainId = legacy->getProperty("id").toString();
    juce::String outputId = "globalOutput";
    for (int suffix = 2; topology.findNode(outputId) != nullptr; ++suffix) {
        outputId = "globalOutput" + juce::String(suffix);
    }
    const auto oldPosition = legacy->getProperty("position").clone();
    auto* insertion = nodeWithId(*nodes, address->destNodeId);
    if (insertion == nullptr) {
        return juce::Result::fail("Gain insertion point is missing");
    }
    const auto gainPosition = insertion->getProperty("position").clone();
    constexpr float clearance = 238.f;
    for (const auto& id : address->excludedEffectNodeIds) {
        shiftRight(*nodeWithId(*nodes, id), clearance);
    }
    auto terminal = std::make_unique<juce::DynamicObject>();
    terminal->setProperty("id", outputId);
    terminal->setProperty("kind", "globalOutput");
    terminal->setProperty("definitionVersion", 1);
    terminal->setProperty("position", oldPosition);
    terminal->setProperty("parameters", juce::var(new juce::DynamicObject()));
    shiftRight(*terminal, clearance);
    legacy->setProperty("kind", "gain");
    legacy->setProperty("position", gainPosition);
    for (auto& encoded : *edges) {
        auto* edge = encoded.getDynamicObject();
        if (edge->getProperty("destNodeId").toString() == gainId) {
            edge->setProperty("destNodeId", outputId);
        }
        if (edge->getProperty("sourceNodeId").toString() == address->sourceNodeId
                && edge->getProperty("sourcePortId").toString() == address->sourcePortId
                && edge->getProperty("destNodeId").toString()
                        == (address->destNodeId == gainId ? outputId : address->destNodeId)
                && edge->getProperty("destPortId").toString() == address->destPortId) {
            edge->setProperty("destNodeId", gainId);
            edge->setProperty("destPortId", "time");
        }
    }
    auto continuation = std::make_unique<juce::DynamicObject>();
    continuation->setProperty("sourceNodeId", gainId);
    continuation->setProperty("sourcePortId", "time");
    continuation->setProperty("destNodeId", address->destNodeId == gainId ? outputId : address->destNodeId);
    continuation->setProperty("destPortId", address->destPortId);
    continuation->setProperty("connectionKind", "signal");
    continuation->setProperty("attachmentType", "none");
    edges->add(juce::var(continuation.release()));
    nodes->add(juce::var(terminal.release()));
    root->setProperty("formatVersion", 10);
    return juce::Result::ok();
}

}

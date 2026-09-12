#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>

#include "Graph/GraphNodeFactory.h"
#include "Graph/NodeDefinition.h"
#include "UI/NodeCanvasScene.h"

using namespace CycleV2;

TEST_CASE("Node definitions have unique coherent schemas", "[cycle-v2][graph][definitions]") {
    const auto& registry = NodeDefinitionRegistry::instance();
    std::set<String> typeIds;

    REQUIRE(registry.definitions().size() == 24);
    for (const auto& definition : registry.definitions()) {
        REQUIRE(definition.typeId.isNotEmpty());
        REQUIRE(definition.defaultInstanceIdPrefix.isNotEmpty());
        REQUIRE(definition.helpText.isNotEmpty());
        REQUIRE(typeIds.insert(definition.typeId).second);
        for (const auto character : definition.helpText) {
            REQUIRE(character < 128);
        }

        std::set<String> portIds;
        for (const auto& port : definition.inputs) {
            REQUIRE(port.input);
            REQUIRE(portIds.insert("in:" + port.id).second);
        }
        for (const auto& port : definition.outputs) {
            REQUIRE_FALSE(port.input);
            REQUIRE(portIds.insert("out:" + port.id).second);
        }

        std::set<String> parameterIds;
        for (const auto& parameter : definition.parameters) {
            REQUIRE(parameterIds.insert(parameter.id).second);
            REQUIRE(parameter.accepts(parameter.defaultValue));
        }

        const Node node = GraphNodeFactory().createNode(definition.kind, "test", {});
        REQUIRE(node.kind == definition.kind);
        REQUIRE(labelForNodeKind(node.kind) == definition.displayName);
        REQUIRE(node.inputs.size() == definition.inputs.size());
        REQUIRE(node.outputs.size() == definition.outputs.size());
    }
}

TEST_CASE("Runtime module metadata comes from node definitions", "[cycle-v2][graph][definitions]") {
    for (const auto& definition : NodeDefinitionRegistry::instance().definitions()) {
        REQUIRE(definition.executable == (definition.audioRole != AudioModuleRole::None));
        if (definition.previewable) {
            REQUIRE(definition.previewRole != PreviewModuleRole::None);
        }
    }
}

TEST_CASE("Pan presents as an inline cable control", "[cycle-v2][graph][definitions]") {
    const Node node = GraphNodeFactory().createNode(NodeKind::SpectralLayer, "layer", {});

    REQUIRE(labelForNodeKind(node.kind) == "Pan");
    REQUIRE(node.parameters.size() == 1);
    REQUIRE(node.parameters.front().id == "pan");
    REQUIRE(node.bounds.getWidth() == 80.f);
    REQUIRE(node.bounds.getHeight() == 80.f);
    REQUIRE(NodeCanvasScene::portWorldCentre(node, node.inputs.front()).getY()
            == node.bounds.getCentreY());
    REQUIRE(NodeCanvasScene::portWorldCentre(node, node.outputs.front()).getY()
            == node.bounds.getCentreY());
}

TEST_CASE("Output owns a unity-default master gain", "[cycle-v2][graph][definitions][output]") {
    const Node node = GraphNodeFactory().createNode(NodeKind::Output, "output", {});

    REQUIRE(node.parameters.size() == 1);
    REQUIRE(node.parameters.front().id == "gain");
    REQUIRE(node.parameters.front().value == "0.5");
    REQUIRE(node.bounds.getWidth() == 190.f);
    REQUIRE(node.bounds.getHeight() == 320.f);
}

TEST_CASE("Global audio graph nodes expose fixed singleton contracts",
        "[cycle-v2][graph][definitions][audio-scope]") {
    const auto& registry = NodeDefinitionRegistry::instance();
    const auto* globalInput = registry.find(NodeKind::GlobalInput);
    const auto* output = registry.find(NodeKind::Output);

    REQUIRE(globalInput != nullptr);
    REQUIRE(globalInput->typeId == "globalInput");
    REQUIRE(globalInput->inputs.empty());
    REQUIRE(globalInput->outputs.size() == 1);
    REQUIRE(globalInput->outputs.front().domain == PortDomain::TimeSignal);
    REQUIRE(globalInput->outputs.front().channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(globalInput->parameters.empty());
    REQUIRE(globalInput->processingCapability == AudioProcessingCapability::GlobalOnly);
    REQUIRE(globalInput->requiredSingleton);
    REQUIRE_FALSE(globalInput->removable);
    REQUIRE(output->processingCapability == AudioProcessingCapability::GlobalOnly);
    REQUIRE(output->requiredSingleton);
    REQUIRE_FALSE(output->removable);
}

TEST_CASE("Effects declare fixed and selectable processing capabilities",
        "[cycle-v2][graph][definitions][audio-scope]") {
    const auto& registry = NodeDefinitionRegistry::instance();

    for (const auto kind : { NodeKind::Delay, NodeKind::Reverb }) {
        const auto* definition = registry.find(kind);
        REQUIRE(definition->processingCapability == AudioProcessingCapability::GlobalOnly);
        REQUIRE(registry.findParameter(kind, "processingScope") == nullptr);
    }

    for (const auto kind : {
            NodeKind::Waveshaper,
            NodeKind::ImpulseResponse,
            NodeKind::Equalizer }) {
        const auto* definition = registry.find(kind);
        const auto* scope = registry.findParameter(kind, "processingScope");
        REQUIRE(definition->processingCapability == AudioProcessingCapability::Selectable);
        REQUIRE(scope != nullptr);
        REQUIRE(scope->defaultValue == "voice");
        REQUIRE(scope->constraint.choices == StringArray { "voice", "global" });
        REQUIRE(hasImpact(scope->impacts, ParameterImpact::GraphSemantics));
        REQUIRE_FALSE(hasImpact(scope->impacts, ParameterImpact::DspConfiguration));
    }
}

TEST_CASE("Required global graph nodes cannot be duplicated or removed",
        "[cycle-v2][graph][editor][audio-scope]") {
    GraphNodeFactory factory;
    GraphEditor editor;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalIn", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));

    REQUIRE_FALSE(editor.addNode(graph, NodeKind::GlobalInput, {}).succeeded());
    REQUIRE_FALSE(editor.addNode(graph, NodeKind::Output, {}).succeeded());
    REQUIRE_FALSE(editor.removeNode(graph, "globalIn").succeeded());
    REQUIRE_FALSE(editor.removeNode(graph, "out").succeeded());
    REQUIRE(graph.getNodes().size() == 2);
}

TEST_CASE("Trimesh owns the spectral range parameter", "[cycle-v2][graph][definitions]") {
    const Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    const auto range = std::find_if(
            node.parameters.begin(),
            node.parameters.end(),
            [](const NodeParameter& parameter) { return parameter.id == "range"; });

    REQUIRE(range != node.parameters.end());
    REQUIRE(range->value == "0.5");
}

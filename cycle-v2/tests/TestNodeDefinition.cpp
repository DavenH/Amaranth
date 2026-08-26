#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphNodeFactory.h"
#include "Graph/NodeDefinition.h"
#include "UI/NodeCanvasScene.h"

#include <set>

using namespace CycleV2;

TEST_CASE("Node definitions have unique coherent schemas", "[cycle-v2][graph][definitions]") {
    const auto& registry = NodeDefinitionRegistry::instance();
    std::set<String> typeIds;

    REQUIRE(registry.definitions().size() == 22);
    for (const auto& definition : registry.definitions()) {
        REQUIRE(definition.typeId.isNotEmpty());
        REQUIRE(definition.defaultInstanceIdPrefix.isNotEmpty());
        REQUIRE(typeIds.insert(definition.typeId).second);

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
    REQUIRE(node.bounds.getWidth() == 80.f);
    REQUIRE(node.bounds.getHeight() == 80.f);
    REQUIRE(NodeCanvasScene::portWorldCentre(node, node.inputs.front()).getY()
            == node.bounds.getCentreY());
    REQUIRE(NodeCanvasScene::portWorldCentre(node, node.outputs.front()).getY()
            == node.bounds.getCentreY());
}

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

#include <Curve/Mesh/Mesh.h>

#include "App/GuideCurveAutomation.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"

using namespace CycleV2;

namespace {

NodeGraph editableMeshGraph() {
    NodeGraph graph;
    auto node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    node.model = TrimeshNodeModelState::copyOf(*mesh, 1);
    mesh->destroy();
    graph.addNode(std::move(node));
    return graph;
}

var requestFor(const GraphDocument& document, const String& command,
        const String& guideId = "guide1") {
    auto object = std::make_unique<DynamicObject>();
    object->setProperty("command", command);
    object->setProperty("guideId", guideId);
    if (command != "inspectGuideCurve") {
        object->setProperty("expectedDocumentRevision", (int64) document.revision());
    }
    if (command == "editGuideCurve") {
        const auto* guide = document.graph().findGuideCurve(guideId);
        object->setProperty("expectedModelRevision", (int64) guide->model->revision());
        object->setProperty("edits", JSON::parse(R"([{"op":"set","vertexId":1,"y":0.3}])"));
    } else if (command == "assignGuideCurve" || command == "detachGuideCurve") {
        object->setProperty("nodeId", "mesh");
        object->setProperty("expectedModelRevision",
                (int64) document.graph().findNode("mesh")->model->revision());
        object->setProperty("selectionIndex", 0);
        object->setProperty("field", "amp");
    }
    return var(object.release());
}

}

TEST_CASE("Guide socket commands inspect edit and attach one existing preset resource headlessly",
        "[cycle-v2][guide-automation]") {
    GraphDocument document(editableMeshGraph());
    GraphCommandDispatcher commands(document);
    const auto created = runGuideCurveCommand(document, commands,
            requestFor(document, "createGuideCurve"));
    REQUIRE((bool) created["ok"]);
    REQUIRE(created["data"]["guideId"].toString() == "guide1");
    REQUIRE(commands.renameGuideCurve("guide1", "Reed impulse").succeeded());
    const String baseline = document.toJson();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    const auto inspected = runGuideCurveCommand(document, commands,
            requestFor(document, "inspectGuideCurve"));
    REQUIRE((bool) inspected["ok"]);
    const auto data = inspected["data"];
    CHECK(data["name"].toString() == "Reed impulse");
    CHECK((int64) data["documentRevision"] == (int64) document.revision());
    CHECK((int) data["resourceRevision"] == 2);
    CHECK((int) data["modelRevision"] == 1);
    REQUIRE(data["points"].size() == 2);
    CHECK((int) data["points"][0]["id"] == 1);
    CHECK((double) data["points"][0]["x"] == Catch::Approx(0.05));
    CHECK((double) data["points"][0]["y"] == Catch::Approx(0.5));
    CHECK((double) data["points"][0]["curve"] == Catch::Approx(1.));
    CHECK(data["assignments"].size() == 0);
    CHECK(document.toJson() == baseline);
    CHECK(publications == 0);

    var edit = requestFor(document, "editGuideCurve");
    edit.getDynamicObject()->setProperty("edits", JSON::parse(R"([
            {"op":"set","vertexId":1,"y":0.3,"curve":0.6},
            {"op":"insert","x":0.5,"y":0.7},
            {"op":"remove","vertexId":2}])"));
    const auto changed = runGuideCurveCommand(document, commands, edit);
    REQUIRE((bool) changed["ok"]);
    CHECK((bool) changed["data"]["changed"]);
    CHECK(publications == 1);
    const String edited = document.toJson();
    const auto current = runGuideCurveCommand(document, commands,
            requestFor(document, "inspectGuideCurve"))["data"];
    CHECK((int) current["resourceRevision"] == 3);
    CHECK((int) current["modelRevision"] == 2);
    CHECK((int) current["points"][1]["id"] == 3);
    CHECK((double) current["points"][0]["y"] == Catch::Approx(0.3));
    CHECK((double) current["points"][0]["curve"] == Catch::Approx(0.6));
    CHECK((double) current["points"][1]["curve"] == Catch::Approx(0.));
    REQUIRE(document.undo());
    CHECK(document.toJson() == baseline);
    REQUIRE(document.redo());
    CHECK(document.toJson() == edited);

    const auto assigned = runGuideCurveCommand(document, commands,
            requestFor(document, "assignGuideCurve"));
    REQUIRE((bool) assigned["ok"]);
    const auto attached = runGuideCurveCommand(document, commands,
            requestFor(document, "inspectGuideCurve"))["data"]["assignments"];
    REQUIRE(attached.size() == 1);
    CHECK(attached[0]["nodeId"].toString() == "mesh");
    CHECK(attached[0]["kind"].toString() == "trimeshCubeComponent");
    CHECK((int) attached[0]["cubeIndex"] == 0);
    CHECK(attached[0]["field"].toString() == "amp");
    REQUIRE(document.undo());
    CHECK(document.toJson() == edited);
    REQUIRE(document.redo());
    const auto detached = runGuideCurveCommand(document, commands,
            requestFor(document, "detachGuideCurve"));
    REQUIRE((bool) detached["ok"]);
    CHECK(document.graph().getGuideAssignments().empty());
    REQUIRE(document.undo());
    CHECK(document.graph().getGuideAssignments().size() == 1);
}

TEST_CASE("Guide socket command validation cannot mutate geometry assignments or history",
        "[cycle-v2][guide-automation][validation]") {
    GraphDocument document(editableMeshGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE((bool) runGuideCurveCommand(document, commands,
            requestFor(document, "createGuideCurve"))["ok"]);
    var attachment = requestFor(document, "assignGuideCurve");
    attachment.getDynamicObject()->setProperty("field", "curve");
    REQUIRE((bool) runGuideCurveCommand(document, commands, attachment)["ok"]);
    REQUIRE(commands.renameGuideCurve("guide1", "redo sentinel").succeeded());
    REQUIRE(document.undo());
    const String baseline = document.toJson();
    const auto revision = document.revision();
    const bool dirty = document.isDirty();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    var request = requestFor(document, "editGuideCurve");
    SECTION("Stale document") {
        request.getDynamicObject()->setProperty("expectedDocumentRevision", (int64) revision - 1);
    }
    SECTION("Coercible document revision") {
        request.getDynamicObject()->setProperty("expectedDocumentRevision", String(revision));
    }
    SECTION("Stale Guide model") {
        request.getDynamicObject()->setProperty("expectedModelRevision", 2);
    }
    SECTION("Coercible point identity") {
        request["edits"][0].getDynamicObject()->setProperty("vertexId", "1");
    }
    SECTION("Fractional point identity") {
        request["edits"][0].getDynamicObject()->setProperty("vertexId", 1.0);
    }
    SECTION("Boolean coordinate") {
        request["edits"][0].getDynamicObject()->setProperty("y", true);
    }
    SECTION("Nonfinite coordinate") {
        request["edits"][0].getDynamicObject()->setProperty("y",
                std::numeric_limits<double>::infinity());
    }
    SECTION("Overflowing coordinate") {
        request["edits"][0].getDynamicObject()->setProperty("y", 1.e300);
    }
    SECTION("Out of range before float narrowing") {
        request["edits"][0].getDynamicObject()->setProperty("y", 1.0000000001);
    }
    SECTION("Negative before float narrowing") {
        request["edits"][0].getDynamicObject()->setProperty("y", -1.e-50);
    }
    SECTION("Unknown operation") {
        request["edits"][0].getDynamicObject()->setProperty("op", "replacePreset");
    }
    SECTION("Stale target model") {
        request = requestFor(document, "assignGuideCurve");
        request.getDynamicObject()->setProperty("expectedModelRevision", 2);
    }
    SECTION("Fractional target selection") {
        request = requestFor(document, "assignGuideCurve");
        request.getDynamicObject()->setProperty("selectionIndex", 0.5);
    }
    SECTION("Negative target selection") {
        request = requestFor(document, "assignGuideCurve");
        request.getDynamicObject()->setProperty("selectionIndex", -1);
    }
    SECTION("Overflowing target selection") {
        request = requestFor(document, "assignGuideCurve");
        request.getDynamicObject()->setProperty("selectionIndex", std::numeric_limits<int64>::max());
    }
    SECTION("Out of bounds target selection") {
        request = requestFor(document, "assignGuideCurve");
        request.getDynamicObject()->setProperty("selectionIndex", 100000);
    }
    SECTION("Unknown assignment field") {
        request = requestFor(document, "assignGuideCurve");
        request.getDynamicObject()->setProperty("field", "unknown");
    }
    SECTION("Unknown detach field must preserve the existing Curve assignment") {
        request = requestFor(document, "detachGuideCurve");
        request.getDynamicObject()->setProperty("field", "unknown");
    }
    const auto response = runGuideCurveCommand(document, commands, request);
    CHECK_FALSE((bool) response["ok"]);
    CHECK(document.toJson() == baseline);
    CHECK(document.revision() == revision);
    CHECK(document.isDirty() == dirty);
    CHECK(document.canRedo());
    CHECK(publications == 0);
}

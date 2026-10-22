#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "App/Automation/TrimeshAuthoringAutomation.h"
#include "Graph/GraphCommandDispatcher.h"
#include "TrimeshAuthoringTestSupport.h"

using namespace CycleV2;
using TestSupport::authoredTrimeshGraph;

namespace {

var requestFor(const GraphDocument& document) {
    auto result = std::make_unique<DynamicObject>();
    result->setProperty("command", "editTrimesh");
    result->setProperty("nodeId", "mesh");
    result->setProperty("expectedDocumentRevision", (int64) document.revision());
    result->setProperty("expectedModelRevision",
            (int64) document.graph().findNode("mesh")->model->revision());
    result->setProperty("edits", JSON::parse(
            R"([{"op":"vertex","vertexIndex":0,"field":"amp","value":0.61}])"));
    return var(result.release());
}

var inspect(GraphDocument& document, GraphCommandDispatcher& commands) {
    const auto result = runTrimeshAuthoringCommand(document, commands,
            JSON::parse(R"({"command":"inspectTrimesh","nodeId":"mesh"})"));
    REQUIRE((bool) result["ok"]);
    return result["data"];
}

}

TEST_CASE("Headless Trimesh inspection exposes revision scoped canonical geometry and Guide ownership",
        "[cycle-v2][trimesh-authoring][automation]") {
    for (const int dimensions : { 0, 1, 2, 3 }) {
        GraphDocument document(authoredTrimeshGraph(dimensions));
        GraphCommandDispatcher commands(document);
        const String baseline = document.toJson();
        const auto data = inspect(document, commands);
        CHECK((int64) data["documentRevision"] == (int64) document.revision());
        CHECK((int) data["modelRevision"] == 1);
        CHECK((int) data["dimensionCount"] == dimensions);
        CHECK((int) data["selectedVertexIndex"] == 8);
        CHECK(data["addressScope"].toString().contains("modelRevision"));
        REQUIRE(data["fields"].size() == 6);
        CHECK((bool) data["fields"][0]["vertexEditable"] == (dimensions >= 1));
        CHECK((bool) data["fields"][1]["vertexEditable"] == (dimensions >= 2));
        CHECK((bool) data["fields"][2]["vertexEditable"] == (dimensions >= 3));
        CHECK((bool) data["fields"][3]["vertexEditable"]);
        CHECK_FALSE((bool) data["fields"][5]["vertexEditable"]);
        CHECK((double) data["fields"][3]["maximum"] == 1.);
        REQUIRE(data["vertices"].size() > 0);
        REQUIRE(data["cubes"].size() > 2);
        CHECK((int) data["vertices"][0]["vertexIndex"] == 0);
        CHECK(data["vertices"][0]["values"].getDynamicObject()->hasProperty("phase"));
        CHECK(data["vertices"][0]["ownerCubeIndices"].size() >= 1);
        CHECK(data["cubes"][0]["vertexIndices"].size() == 8);
        CHECK(data["cubes"][2]["guideAssignments"]["amp"].toString() == "guide1");
        CHECK(data["guideGainScope"].toString().contains("incident cubes"));
        CHECK(document.toJson() == baseline);
        CHECK_FALSE(document.canUndo());
    }
}

TEST_CASE("Trimesh socket batches edit explicit vertices cubes and Guide gains with one undo",
        "[cycle-v2][trimesh-authoring][automation]") {
    GraphDocument document(authoredTrimeshGraph());
    GraphCommandDispatcher commands(document);
    const String baseline = document.toJson();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    var request = requestFor(document);
    request.getDynamicObject()->setProperty("edits", JSON::parse(R"([
            {"op":"vertex","vertexIndex":0,"field":"amp","value":0.61},
            {"op":"cubeCurve","cubeIndex":1,"value":0.35},
            {"op":"guideGain","vertexIndex":16,"field":"amp","value":0.77}])"));
    const auto result = runTrimeshAuthoringCommand(document, commands, request);
    REQUIRE((bool) result["ok"]);
    CHECK((bool) result["data"]["changed"]);
    CHECK((int) result["data"]["modelRevision"] == 2);
    CHECK((int64) result["data"]["documentRevision"] == (int64) document.revision());
    CHECK(publications == 1);
    const auto data = inspect(document, commands);
    CHECK((double) data["vertices"][0]["values"]["amp"] == Catch::Approx(0.61));
    CHECK((double) data["cubes"][2]["guideGains"]["amp"] == Catch::Approx(0.77));
    CHECK((int) data["selectedVertexIndex"] == 8);
    const String edited = document.toJson();
    REQUIRE(document.undo());
    CHECK(document.toJson() == baseline);
    CHECK_FALSE(document.canUndo());
    REQUIRE(document.redo());
    CHECK(document.toJson() == edited);
}

TEST_CASE("Malformed Trimesh socket requests preserve state and redo without publication",
        "[cycle-v2][trimesh-authoring][automation][validation]") {
    GraphDocument document(authoredTrimeshGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setPresetDescription("redo sentinel").succeeded());
    REQUIRE(document.undo());
    const String baseline = document.toJson();
    const auto revision = document.revision();
    const auto dirty = document.isDirty();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    var request = requestFor(document);
    SECTION("Node identity must be typed") { request.getDynamicObject()->setProperty("nodeId", 1); }
    SECTION("Revision cannot be a string") {
        request.getDynamicObject()->setProperty("expectedDocumentRevision", String(revision));
    }
    SECTION("Stale model") { request.getDynamicObject()->setProperty("expectedModelRevision", 2); }
    SECTION("Floating point index") {
        request["edits"][0].getDynamicObject()->setProperty("vertexIndex", 0.0);
    }
    SECTION("Boolean index") { request["edits"][0].getDynamicObject()->setProperty("vertexIndex", false); }
    SECTION("Overflowing index") {
        request["edits"][0].getDynamicObject()->setProperty("vertexIndex", std::numeric_limits<int64>::max());
    }
    SECTION("Boolean value") { request["edits"][0].getDynamicObject()->setProperty("value", true); }
    SECTION("String value") { request["edits"][0].getDynamicObject()->setProperty("value", "0.5"); }
    SECTION("Nonfinite value") {
        request["edits"][0].getDynamicObject()->setProperty("value", std::numeric_limits<double>::infinity());
    }
    SECTION("Out of range before float narrowing") {
        request["edits"][0].getDynamicObject()->setProperty("value", 1.0000000001);
    }
    SECTION("Negative before float narrowing") {
        request["edits"][0].getDynamicObject()->setProperty("value", -1.e-50);
    }
    SECTION("Unknown operation") { request["edits"][0].getDynamicObject()->setProperty("op", "replace"); }
    SECTION("Unknown operation field") {
        request["edits"][0].getDynamicObject()->setProperty("amplitude", 0.7);
    }
    SECTION("Explicit cube curve cannot have a vertex address") {
        request.getDynamicObject()->setProperty("edits", JSON::parse(
                R"([{"op":"cubeCurve","cubeIndex":0,"vertexIndex":0,"value":0.5}])"));
    }
    SECTION("Explicit cube curve cannot have a vertex field") {
        request.getDynamicObject()->setProperty("edits", JSON::parse(
                R"([{"op":"cubeCurve","cubeIndex":0,"field":"curve","value":0.5}])"));
    }
    SECTION("Vertex curve must not alias a cube average") {
        request["edits"][0].getDynamicObject()->setProperty("field", "curve");
    }
    SECTION("Invalid operation after a valid one") {
        request["edits"].getArray()->add(JSON::parse(
                R"({"op":"cubeCurve","cubeIndex":100000,"value":0.5})"));
    }
    const auto result = runTrimeshAuthoringCommand(document, commands, request);
    CHECK_FALSE((bool) result["ok"]);
    CHECK(document.toJson() == baseline);
    CHECK(document.revision() == revision);
    CHECK(document.isDirty() == dirty);
    CHECK(document.canRedo());
    CHECK(publications == 0);
}

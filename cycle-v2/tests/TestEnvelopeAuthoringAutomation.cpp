#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "App/EnvelopeAuthoringAutomation.h"
#include "EnvelopeAuthoringTestSupport.h"

using namespace CycleV2;
using namespace CycleV2::TestSupport;

namespace {

var requestFor(const GraphDocument& document) {
    auto object = std::make_unique<DynamicObject>();
    object->setProperty("command", "editEnvelope");
    object->setProperty("nodeId", "env");
    object->setProperty("expectedDocumentRevision", (int64) document.revision());
    object->setProperty("expectedModelRevision", (int64) document.graph().findNode("env")->model->revision());
    object->setProperty("dimensionCount", 3);
    object->setProperty("linkedAxes", JSON::parse(R"({"yellow":true,"red":true,"blue":true})"));
    var edit = JSON::parse(R"({"op":"value","corner":0,"field":"amp","value":0.61})");
    edit.getDynamicObject()->setProperty("cubeId", (int64) envelopeFor(document).getCubeIds()[1]);
    Array<var> edits;
    edits.add(edit);
    object->setProperty("edits", edits);
    return var(object.release());
}

}

TEST_CASE("Envelope inspection exposes stable full geometry Guide ownership and raw units",
        "[cycle-v2][envelope-authoring][automation]") {
    EnvelopeAuthoringCurveTables tables;
    for (const int dimensions : { 0, 1, 2, 3 }) {
        GraphDocument document(authoredEnvelopeGraph(dimensions));
        GraphCommandDispatcher commands(document);
        const String baseline = document.toJson();
        const auto result = runEnvelopeAuthoringCommand(document, commands,
                JSON::parse(R"({"command":"inspectEnvelope","nodeId":"env"})"));
        REQUIRE((bool) result["ok"]);
        const auto data = result["data"];
        CHECK((int64) data["documentRevision"] == (int64) document.revision());
        CHECK((int) data["modelRevision"] == 1);
        CHECK((int) data["dimensionCount"] == dimensions);
        CHECK(data["purpose"].toString() == "control");
        CHECK((double) data["phaseMaximum"] == 10.);
        CHECK(data["phaseUnits"].toString().contains("Voice Context"));
        CHECK((double) data["authoredMorph"]["yellow"] == Catch::Approx(0.4));
        CHECK((double) data["authoredMorph"]["red"] == Catch::Approx(0.3));
        CHECK((double) data["authoredMorph"]["blue"] == Catch::Approx(0.7));
        REQUIRE(data["cubes"].size() == (int) envelopeFor(document).getCubeIds().size());
        CHECK((int64) data["cubes"][0]["cubeId"] == (int64) envelopeFor(document).getCubeIds()[0]);
        CHECK((int) data["cubes"][0]["cubeIndex"] == 0);
        REQUIRE(data["cubes"][0]["corners"].size() == 8);
        CHECK((int) data["cubes"][0]["corners"][7]["corner"] == 7);
        CHECK(data["cubes"][2]["guides"]["amp"].toString() == "guide1");
        CHECK((bool) data["cubes"][3]["sustain"]);
        CHECK_FALSE((bool) data["cubes"][3]["loop"]);
        CHECK((double) data["cubes"][2]["guideGains"]["amp"] == 0.5);
        CHECK(document.toJson() == baseline);
        CHECK_FALSE(document.canUndo());
    }
}

TEST_CASE("Envelope socket edits are addressed atomic batches with one publication",
        "[cycle-v2][envelope-authoring][automation]") {
    EnvelopeAuthoringCurveTables tables;
    GraphDocument document(authoredEnvelopeGraph());
    GraphCommandDispatcher commands(document);
    const String baseline = document.toJson();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    auto request = requestFor(document);
    var release = JSON::parse(R"({"op":"value","corner":0,"field":"phase","value":1.75})");
    release.getDynamicObject()->setProperty("cubeId", (int64) envelopeFor(document).getCubeIds()[5]);
    request["edits"].getArray()->add(release);
    const auto result = runEnvelopeAuthoringCommand(document, commands, request);
    REQUIRE((bool) result["ok"]);
    CHECK((bool) result["data"]["changed"]);
    CHECK((int) result["data"]["modelRevision"] == 2);
    CHECK((int64) result["data"]["documentRevision"] == (int64) document.revision());
    CHECK((double) result["data"]["cubes"][1]["corners"][0]["amp"] == Catch::Approx(0.61));
    CHECK((double) result["data"]["cubes"][5]["corners"][0]["phase"] == 1.75);
    CHECK(publications == 1);
    const String edited = document.toJson();
    REQUIRE(document.undo());
    CHECK(document.toJson() == baseline);
    CHECK_FALSE(document.canUndo());
    REQUIRE(document.redo());
    CHECK(document.toJson() == edited);
}

TEST_CASE("Malformed Envelope socket requests preserve geometry history and redo",
        "[cycle-v2][envelope-authoring][automation][validation]") {
    EnvelopeAuthoringCurveTables tables;
    GraphDocument document(authoredEnvelopeGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setPresetDescription("redo sentinel").succeeded());
    REQUIRE(document.undo());
    const String baseline = document.toJson();
    const auto revision = document.revision();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    auto request = requestFor(document);
    SECTION("Numeric node identity") { request.getDynamicObject()->setProperty("nodeId", 1); }
    SECTION("String revision") { request.getDynamicObject()->setProperty("expectedDocumentRevision", "1"); }
    SECTION("Boolean revision") { request.getDynamicObject()->setProperty("expectedModelRevision", true); }
    SECTION("Incomplete links") { request["linkedAxes"].getDynamicObject()->removeProperty("blue"); }
    SECTION("Nonboolean links") { request["linkedAxes"].getDynamicObject()->setProperty("red", 1); }
    SECTION("Unknown link") { request["linkedAxes"].getDynamicObject()->setProperty("green", false); }
    SECTION("Missing dimensions") { request.getDynamicObject()->removeProperty("dimensionCount"); }
    SECTION("Floating cube identity") { request["edits"][0].getDynamicObject()->setProperty("cubeId", 2.0); }
    SECTION("Boolean corner") { request["edits"][0].getDynamicObject()->setProperty("corner", false); }
    SECTION("Invalid corner") { request["edits"][0].getDynamicObject()->setProperty("corner", 8); }
    SECTION("Unknown field") { request["edits"][0].getDynamicObject()->setProperty("field", "unknown"); }
    SECTION("Boolean value") { request["edits"][0].getDynamicObject()->setProperty("value", true); }
    SECTION("String value") { request["edits"][0].getDynamicObject()->setProperty("value", "0.5"); }
    SECTION("Nonfinite value") {
        request["edits"][0].getDynamicObject()->setProperty("value", std::numeric_limits<double>::infinity());
    }
    SECTION("Out of range before float narrowing") {
        request["edits"][0].getDynamicObject()->setProperty("value", 1.0000000001);
    }
    SECTION("Negative before float narrowing") { request["edits"][0].getDynamicObject()->setProperty("value", -1.e-50); }
    SECTION("Guide gains cannot carry corner addresses") {
        request["edits"][0].getDynamicObject()->setProperty("op", "guideGain");
    }
    SECTION("Markers cannot carry vertex data") { request["edits"][0].getDynamicObject()->setProperty("op", "loop"); }
    SECTION("Invalid operation after a valid edit") {
        request["edits"].getArray()->add(JSON::parse(R"({"op":"delete","cubeId":1})"));
    }
    const auto result = runEnvelopeAuthoringCommand(document, commands, request);
    CHECK_FALSE((bool) result["ok"]);
    CHECK(document.toJson() == baseline);
    CHECK(document.revision() == revision);
    CHECK(document.canRedo());
    CHECK(publications == 0);
}

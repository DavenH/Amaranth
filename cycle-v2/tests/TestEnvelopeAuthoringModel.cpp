#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "EnvelopeAuthoringTestSupport.h"
#include "Graph/NodeModelEnvelopeCodec.h"

using namespace CycleV2;
using namespace CycleV2::TestSupport;

namespace {

void addLine(EnvelopeNodeModel& model) {
    auto& mesh = model.getMesh();
    auto* cube = new VertCube(&mesh);
    MorphPosition position(0.f, 0.f, 0.f);
    position.timeDepth = 1.f;
    position.redDepth = 1.f;
    position.blueDepth = 1.f;
    cube->initLine(position, 0.5f, 0.6f, 0.5f);
    mesh.addCube(cube);
}

var codecEnvelope(const var& model) {
    return NodeModelEnvelopeCodec::write("envelope", EnvelopeNodeModel::currentVersion,
            (uint64_t) (int64) model["revision"], "state", model);
}

}

TEST_CASE("Envelope saved identity high water survives deletion and an empty model",
        "[cycle-v2][envelope-authoring][codec]") {
    EnvelopeAuthoringCurveTables tables;
    EnvelopeNodeModel model;
    const auto removedId = model.getCubeIds().back();
    SECTION("Delete highest live identity") {
        auto& mesh = model.getMesh();
        auto* removed = mesh.getCubes().back();
        std::vector<Vertex*> detached;
        REQUIRE(mesh.detachCube(removed, detached));
        for (auto* vertex : detached) {
            delete vertex;
        }
        delete removed;
    }
    SECTION("Delete every cube") {
        auto& mesh = model.getMesh();
        mesh.loopCubes.clear();
        mesh.sustainCubes.clear();
        mesh.destroy();
    }
    REQUIRE(model.synchronizeFromMesh(nullptr));
    const auto saved = JSON::parse(JSON::toString(model.writeJSON()));
    CHECK((int64) saved["nextCubeIdentity"] > (int64) removedId);
    EnvelopeNodeModel restored;
    REQUIRE(restored.readJSON(saved));
    CHECK(restored.equals(model));
    String error;
    REQUIRE(CurveNodeDomainCodec(NodeKind::Envelope).readAuthoringJSON(codecEnvelope(saved), error) != nullptr);
    addLine(restored);
    REQUIRE(restored.synchronizeFromMesh(nullptr));
    CHECK(restored.getCubeIds().back() > removedId);
}

TEST_CASE("Envelope legacy snapshots derive identity high water and empty snapshots round trip",
        "[cycle-v2][envelope-authoring][codec]") {
    EnvelopeAuthoringCurveTables tables;
    EnvelopeNodeModel model;
    SECTION("Existing v2 snapshot without a counter") {}
    SECTION("Empty legacy model") {
        model.getMesh().loopCubes.clear();
        model.getMesh().sustainCubes.clear();
        model.getMesh().destroy();
        REQUIRE(model.synchronizeFromMesh(nullptr));
    }
    auto legacy = model.writeJSON();
    legacy.getDynamicObject()->removeProperty("nextCubeIdentity");
    EnvelopeNodeModel restored;
    REQUIRE(restored.readJSON(legacy));
    const auto previous = restored.getCubeIds();
    addLine(restored);
    REQUIRE(restored.synchronizeFromMesh(nullptr));
    CHECK(restored.getCubeIds().back() == (previous.empty() ? 1 : previous.back() + 1));
    EnvelopeNodeModel roundTripped;
    REQUIRE(roundTripped.readJSON(restored.writeJSON()));
    CHECK(roundTripped.equals(restored));
}

TEST_CASE("Envelope identity counters reject malformed values through model and authoring codec",
        "[cycle-v2][envelope-authoring][codec][validation]") {
    EnvelopeAuthoringCurveTables tables;
    EnvelopeNodeModel baseline;
    auto encoded = baseline.writeJSON();
    SECTION("Zero") { encoded.getDynamicObject()->setProperty("nextCubeIdentity", 0); }
    SECTION("Negative") { encoded.getDynamicObject()->setProperty("nextCubeIdentity", -1); }
    SECTION("Boolean") { encoded.getDynamicObject()->setProperty("nextCubeIdentity", true); }
    SECTION("Floating point") { encoded.getDynamicObject()->setProperty("nextCubeIdentity", 8.0); }
    SECTION("String") { encoded.getDynamicObject()->setProperty("nextCubeIdentity", "8"); }
    SECTION("Reuses a live identity") {
        encoded.getDynamicObject()->setProperty("nextCubeIdentity", (int64) baseline.getCubeIds().back());
    }
    SECTION("Missing identity for live geometry") { encoded["cubeIds"].getArray()->clear(); }
    SECTION("Nonpositive cube identity") { encoded["cubeIds"].getArray()->getReference(0) = -1; }
    SECTION("No room for a successor") {
        encoded["cubeIds"].getArray()->getReference(0) = std::numeric_limits<int64>::max();
    }
    EnvelopeNodeModel restored;
    REQUIRE_FALSE(restored.readJSON(encoded));
    CHECK(restored.equals(baseline));
    CurveNodeDomainCodec codec(NodeKind::Envelope);
    String decodeError;
    CHECK(codec.readJSON(codecEnvelope(encoded), decodeError) == nullptr);
    String authoringError;
    CHECK(codec.readAuthoringJSON(codecEnvelope(encoded), authoringError) == nullptr);
}

TEST_CASE("Envelope exhausted identity high water rejects construction without overflow",
        "[cycle-v2][envelope-authoring][codec][validation]") {
    EnvelopeAuthoringCurveTables tables;
    EnvelopeNodeModel model;
    auto encoded = model.writeJSON();
    encoded.getDynamicObject()->setProperty("nextCubeIdentity", std::numeric_limits<int64>::max());
    REQUIRE(model.readJSON(encoded));
    const auto identities = model.getCubeIds();
    addLine(model);
    CHECK_FALSE(model.synchronizeFromMesh(nullptr));
    CHECK(model.getCubeIds() == identities);
    CHECK((int64) model.writeJSON()["nextCubeIdentity"] == std::numeric_limits<int64>::max());
}

TEST_CASE("Envelope explicit empty sustain survives copying while legacy absence gets a fallback",
        "[cycle-v2][envelope-authoring][markers][codec]") {
    EnvelopeAuthoringCurveTables tables;
    EnvelopeNodeModel original;
    original.getMesh().sustainCubes.clear();
    REQUIRE(original.synchronizeFromMesh(nullptr));
    EnvelopeNodeModel copied;
    REQUIRE(copied.copyFrom(original));
    CHECK(copied.getMesh().sustainCubes.empty());
    EnvelopeNodeModel loaded;
    const auto encoded = original.writeJSON();
    REQUIRE(loaded.readJSON(encoded));
    CHECK(loaded.getMesh().sustainCubes.empty());
    auto legacy = JSON::parse(JSON::toString(encoded));
    legacy["mesh"].getDynamicObject()->removeProperty("sustainIndices");
    REQUIRE(loaded.readJSON(legacy));
    CHECK_FALSE(loaded.getMesh().sustainCubes.empty());

    XmlElement xml("MeshCopy");
    original.getMesh().writeXML(&xml);
    auto* envelope = xml.getChildByName("EnvelopeMesh");
    REQUIRE(envelope != nullptr);
    envelope->removeChildElement(envelope->getChildByName("SustIndices"), true);
    EnvelopeMesh legacyXml("LegacyEnvelope");
    REQUIRE(legacyXml.readXML(&xml));
    CHECK_FALSE(legacyXml.sustainCubes.empty());
    legacyXml.destroy();

    envelope->removeChildElement(envelope->getChildByName("LoopIndices"), true);
    envelope->setAttribute("sustainIndex", 1);
    envelope->setAttribute("sustainIndex2", 2);
    REQUIRE(legacyXml.readXML(&xml));
    REQUIRE(legacyXml.loopCubes.count(legacyXml.getCubes()[1]) == 1);
    REQUIRE(legacyXml.sustainCubes.count(legacyXml.getCubes()[2]) == 1);
    for (const int index : { 1, 2 }) {
        for (auto* vertex : legacyXml.getCubes()[(size_t) index]->lineVerts) {
            CHECK(vertex->values[Vertex::Curve] == 1.f);
        }
    }
    legacyXml.destroy();
}

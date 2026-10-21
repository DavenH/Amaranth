#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "Graph/NodeDefinition.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/ModelAuthoringValidation.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"

using namespace CycleV2;

namespace {

var populatedMeshModel() {
    auto mesh = TrimeshMeshFactory::createDefaultMesh("authoring-validation");
    const auto model = TrimeshNodeModelState::copyOf(*mesh, 2);
    mesh->destroy();
    return model->writeJSON();
}

void expectRejected(const NodeModelCodec& codec, const var& value) {
    String error;
    REQUIRE(codec.readAuthoringJSON(value, error) == nullptr);
    REQUIRE(error.isNotEmpty());
}

}

TEST_CASE("Authored model decoding preserves canonical defaults and ignores unknown fields",
        "[cycle-v2][model-authoring][defaults]") {
    int supported = 0;
    for (const auto& definition : NodeDefinitionRegistry::instance().definitions()) {
        const auto& codec = definition.modelCodec;
        if (codec == nullptr) {
            continue;
        }
        const auto model = codec->createDefault();
        REQUIRE(model != nullptr);
        String error;
        if (!codec->supportsAuthoring()) {
            REQUIRE(codec->readAuthoringJSON(model->writeJSON(), error) == nullptr);
            continue;
        }
        ++supported;
        INFO(definition.typeId);
        var encoded = model->writeJSON();
        encoded.getDynamicObject()->setProperty("futureMetadata", "preserve loader compatibility");
        auto payload = encoded[encoded.hasProperty("mesh") ? "mesh" : "state"];
        payload.getDynamicObject()->setProperty("futureGeometryMetadata", 123);
        const auto authored = codec->readAuthoringJSON(encoded, error);
        REQUIRE(authored != nullptr);
        REQUIRE(authored->equals(*model));
    }
    REQUIRE(supported >= 4);
    REQUIRE_FALSE(GuideCurveModelCodec().supportsAuthoring());
}

TEST_CASE("Authored replacements accept existing factory and legacy model geometry",
        "[cycle-v2][model-authoring][presets]") {
    const File directory = File(CYCLE_V2_SOURCE_DIR).getChildFile("content/presets");
    int models = 0;
    bool checkedLegacy = false;
    for (const auto& file : directory.findChildFiles(File::findFiles, true, "*.cyclegraph")) {
        const var graph = JSON::parse(file);
        const auto* nodes = graph["nodes"].getArray();
        REQUIRE(nodes != nullptr);
        for (const auto& node : *nodes) {
            const auto* definition = NodeDefinitionRegistry::instance().find(node["kind"].toString());
            if (definition == nullptr || definition->modelCodec == nullptr
                    || !definition->modelCodec->supportsAuthoring() || !node.hasProperty("model")) {
                continue;
            }
            INFO(file.getRelativePathFrom(directory) + ": " + node["id"].toString());
            String error;
            const auto loaded = definition->modelCodec->readJSON(node["model"], error);
            REQUIRE(loaded != nullptr);
            const auto authored = definition->modelCodec->readAuthoringJSON(node["model"], error);
            INFO(error);
            REQUIRE(authored != nullptr);
            REQUIRE(authored->equals(*loaded));
            ++models;
            checkedLegacy = checkedLegacy || file.getParentDirectory().getFileName() == "old";
        }
    }
    REQUIRE(models > 1000);
    REQUIRE(checkedLegacy);
}

TEST_CASE("Authoring rejects coercible curve fields without changing persistence acceptance",
        "[cycle-v2][model-authoring][curve]") {
    CurveNodeDomainCodec codec(NodeKind::Waveshaper);
    const var baseline = codec.createDefault()->writeJSON();
    var coercible = baseline.clone();
    auto& first = coercible["state"]["vertices"].getArray()->getReference(0);
    first.getDynamicObject()->setProperty("x", "0.25");
    String error;
    REQUIRE(codec.readJSON(coercible, error) != nullptr);
    expectRejected(codec, coercible);

    SECTION("Missing required coordinates cannot become zero") {
        var invalid = baseline.clone();
        invalid["state"]["vertices"].getArray()->getReference(0)
                .getDynamicObject()->removeProperty("curve");
        REQUIRE(codec.readJSON(invalid, error) != nullptr);
        expectRejected(codec, invalid);
    }
    SECTION("Vertex identities remain positive and unique") {
        for (const var id : { var((int64) 0), var((int64) -1),
                baseline["state"]["vertices"].getArray()->getReference(1)["id"] }) {
            var invalid = baseline.clone();
            invalid["state"]["vertices"].getArray()->getReference(0)
                    .getDynamicObject()->setProperty("id", id);
            expectRejected(codec, invalid);
        }
    }
    SECTION("Coordinates remain finite and normalized") {
        for (const double coordinate : { std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::infinity(), -0.1, 1.1 }) {
            var invalid = baseline.clone();
            invalid["state"]["vertices"].getArray()->getReference(0)
                    .getDynamicObject()->setProperty("y", coordinate);
            expectRejected(codec, invalid);
        }
    }
    SECTION("Envelope schema and both revisions remain exact") {
        for (const char* field : { "schema", "version", "revision" }) {
            var invalid = baseline.clone();
            invalid.getDynamicObject()->setProperty(field, "1");
            expectRejected(codec, invalid);
        }
        var invalid = baseline.clone();
        invalid["state"].getDynamicObject()->setProperty("revision", (int64) 2);
        expectRejected(codec, invalid);
        invalid = baseline.clone();
        invalid["state"].getDynamicObject()->setProperty("selection", "1");
        expectRejected(codec, invalid);
    }
}

TEST_CASE("Authored mesh decoding rejects malformed storage and cube references",
        "[cycle-v2][model-authoring][trimesh]") {
    TrimeshNodeModelCodec codec;
    const var baseline = populatedMeshModel();
    String error;
    REQUIRE(codec.readAuthoringJSON(baseline, error) != nullptr);

    SECTION("Legacy coordinate overshoot remains representable") {
        var compatible = baseline.clone();
        auto* vertex = compatible["mesh"]["vertices"].getArray()->getReference(0).getDynamicObject();
        vertex->setProperty("phase", -0.84);
        vertex->setProperty("time", 1.035);
        vertex->setProperty("amp", -0.026);
        REQUIRE(codec.readAuthoringJSON(compatible, error) != nullptr);
    }
    SECTION("Persistence alone may default missing values") {
        var invalid = baseline.clone();
        invalid["mesh"]["vertices"].getArray()->getReference(0)
                .getDynamicObject()->removeProperty("amp");
        REQUIRE(codec.readJSON(invalid, error) != nullptr);
        expectRejected(codec, invalid);
    }
    SECTION("Unsafe numeric storage is rejected before reconstruction") {
        for (const var coordinate : { var("0.5"), var(true),
                var(std::numeric_limits<double>::quiet_NaN()),
                var(std::numeric_limits<double>::infinity()),
                var(std::numeric_limits<double>::max()) }) {
            var invalid = baseline.clone();
            invalid["mesh"]["vertices"].getArray()->getReference(0)
                    .getDynamicObject()->setProperty("phase", coordinate);
            expectRejected(codec, invalid);
        }
    }
    SECTION("Vertex identities are unique") {
        var invalid = baseline.clone();
        auto* vertices = invalid["mesh"]["vertices"].getArray();
        vertices->getReference(1).getDynamicObject()->setProperty("id", vertices->getReference(0)["id"]);
        expectRejected(codec, invalid);
    }
    SECTION("Every cube names eight distinct existing vertices") {
        for (const var id : { var(-1), var(1000000), var("2"), var(0) }) {
            var invalid = baseline.clone();
            auto* ids = invalid["mesh"]["cubes"].getArray()->getReference(0)["vertexIds"].getArray();
            ids->getReference(1) = id;
            expectRejected(codec, invalid);
        }
        var invalid = baseline.clone();
        invalid["mesh"]["cubes"].getArray()->getReference(0)["vertexIds"].getArray()->removeLast();
        expectRejected(codec, invalid);
    }
    SECTION("Optional guide storage cannot silently coerce malformed fields") {
        for (const char* field : { "guides", "gains" }) {
            var invalid = baseline.clone();
            invalid["mesh"]["cubes"].getArray()->getReference(0)
                    .getDynamicObject()->setProperty(field, 7);
            expectRejected(codec, invalid);
        }
        var invalid = baseline.clone();
        invalid["mesh"]["cubes"].getArray()->getReference(0)["guides"]
                .getDynamicObject()->setProperty("phase", 128);
        expectRejected(codec, invalid);
        invalid = baseline.clone();
        invalid["mesh"]["cubes"].getArray()->getReference(0)["gains"]
                .getDynamicObject()->setProperty("phase", "0.5");
        expectRejected(codec, invalid);
    }
}

TEST_CASE("Envelope authoring validates cube identities and loop indices",
        "[cycle-v2][model-authoring][envelope]") {
    CurveNodeDomainCodec codec(NodeKind::Envelope);
    const var baseline = codec.createDefault()->writeJSON();

    for (const char* field : { "loopIndices", "sustainIndices" }) {
        var invalid = baseline.clone();
        invalid["state"]["mesh"].getDynamicObject()->setProperty(field, var(Array<var> { 1000000 }));
        expectRejected(codec, invalid);
        invalid = baseline.clone();
        invalid["state"]["mesh"].getDynamicObject()->setProperty(field, var(Array<var> { 0, 0 }));
        expectRejected(codec, invalid);
    }
    var invalid = baseline.clone();
    invalid["state"]["cubeIds"].getArray()->getReference(0) = (int64) 0;
    expectRejected(codec, invalid);
    invalid = baseline.clone();
    invalid["state"].getDynamicObject()->setProperty("red", 2.0);
    expectRejected(codec, invalid);
    invalid = baseline.clone();
    invalid["state"].getDynamicObject()->setProperty("logarithmic", "false");
    expectRejected(codec, invalid);
}

TEST_CASE("Authored geometry payloads have bounded point and cube counts",
        "[cycle-v2][model-authoring][limits]") {
    CurveNodeDomainCodec curveCodec(NodeKind::Waveshaper);
    var curve = curveCodec.createDefault()->writeJSON();
    auto* points = curve["state"]["vertices"].getArray();
    const var point = points->getReference(0).clone();
    points->clear();
    for (int index = 0; index <= ModelAuthoringValidation::maximumVertices; ++index) {
        var next = point.clone();
        next.getDynamicObject()->setProperty("id", (int64) index + 1);
        points->add(next);
    }
    expectRejected(curveCodec, curve);
    points->removeLast();
    String error;
    REQUIRE(curveCodec.readAuthoringJSON(curve, error) != nullptr);

    TrimeshNodeModelCodec meshCodec;
    var mesh = populatedMeshModel();
    auto* cubes = mesh["mesh"]["cubes"].getArray();
    const var cube = cubes->getReference(0).clone();
    while (cubes->size() <= ModelAuthoringValidation::maximumCubes) {
        cubes->add(cube.clone());
    }
    expectRejected(meshCodec, mesh);

    mesh = populatedMeshModel();
    auto* vertices = mesh["mesh"]["vertices"].getArray();
    const var vertex = vertices->getReference(0).clone();
    while (vertices->size() <= ModelAuthoringValidation::maximumVertices) {
        var next = vertex.clone();
        next.getDynamicObject()->setProperty("id", vertices->size());
        vertices->add(next);
    }
    expectRejected(meshCodec, mesh);
}

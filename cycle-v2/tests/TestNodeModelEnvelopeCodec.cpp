#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <memory>
#include <vector>

#include "Graph/NodeDefinition.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Nodes/Unison/UnisonNode.h"

using namespace CycleV2;

TEST_CASE("Typed node model codecs share envelope validation",
        "[cycle-v2][graph][node-model][codec]") {
    struct CodecCase {
        const char* name;
        std::unique_ptr<NodeModelCodec> codec;
    };
    std::vector<CodecCase> codecs;
    codecs.push_back({
            "flat curve",
            std::make_unique<CurveNodeDomainCodec>(NodeKind::Waveshaper) });
    codecs.push_back({
            "Envelope",
            std::make_unique<CurveNodeDomainCodec>(NodeKind::Envelope) });
    codecs.push_back({ "Guide Curve", std::make_unique<GuideCurveModelCodec>() });
    codecs.push_back({ "Trimesh", std::make_unique<TrimeshNodeModelCodec>() });
    codecs.push_back({ "Unison", std::make_unique<UnisonNodeModelCodec>() });

    struct Malformation {
        const char* name;
        std::function<void(DynamicObject&)> apply;
    };
    const std::vector<Malformation> malformations {
            {
                    "schema",
                    [](DynamicObject& object) {
                        object.setProperty("schema", "wrong-schema");
                    }
            },
            {
                    "version",
                    [](DynamicObject& object) {
                        object.setProperty(
                                "version",
                                (int) object.getProperty("version") + 1);
                    }
            },
            {
                    "revision",
                    [](DynamicObject& object) {
                        object.setProperty("revision", 0);
                    }
            }
    };

    for (const auto& codecCase : codecs) {
        const NodeModelStatePtr model = codecCase.codec->createDefault();
        REQUIRE(model != nullptr);
        String validError;
        REQUIRE(codecCase.codec->readJSON(model->writeJSON(), validError) != nullptr);
        for (const auto& malformation : malformations) {
            INFO(codecCase.name << " with invalid " << malformation.name);
            var encoded = JSON::parse(JSON::toString(model->writeJSON(), false));
            DynamicObject* object = encoded.getDynamicObject();
            REQUIRE(object != nullptr);
            malformation.apply(*object);

            String error;
            REQUIRE(codecCase.codec->readJSON(encoded, error) == nullptr);
            REQUIRE(error.isNotEmpty());
        }
    }
}

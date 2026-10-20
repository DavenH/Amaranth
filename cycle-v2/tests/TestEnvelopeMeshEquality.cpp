#include <catch2/catch_test_macros.hpp>

#include <Curve/Mesh/EnvelopeMesh.h>

TEST_CASE("Envelope equality compares loop and sustain membership independently of allocation order",
        "[cycle-v2][envelope][mesh-equality]") {
    const auto file = juce::File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content/presets/electric-1.cyclegraph");
    const juce::var graph = juce::JSON::parse(file);
    const auto* nodes = graph["nodes"].getArray();
    REQUIRE(nodes != nullptr);
    juce::var encoded;
    for (const auto& node : *nodes) {
        if (node["id"].toString() == "volumeEnvelope1") {
            encoded = node["model"]["state"]["mesh"];
        }
    }
    REQUIRE(encoded.getDynamicObject() != nullptr);
    EnvelopeMesh first("first");
    EnvelopeMesh second("second");
    REQUIRE(first.readJSON(encoded));
    REQUIRE(second.readJSON(encoded));
    REQUIRE(first.sustainCubes.size() > 1);
    REQUIRE(first.equals(second));
    REQUIRE(second.equals(first));

    first.loopCubes.insert(first.getCubes()[0]);
    first.loopCubes.insert(first.getCubes()[1]);
    second.loopCubes.insert(second.getCubes()[1]);
    second.loopCubes.insert(second.getCubes()[0]);
    REQUIRE(first.equals(second));
    second.loopCubes.erase(second.getCubes()[1]);
    REQUIRE_FALSE(first.equals(second));
    second.loopCubes.insert(second.getCubes()[1]);
    REQUIRE(first.equals(second));
    second.sustainCubes.erase(*second.sustainCubes.begin());
    REQUIRE_FALSE(first.equals(second));
    first.destroy();
    second.destroy();
}

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Graph/GraphSerializer.h"
#include "Graph/NodeParameterMap.h"

using namespace CycleV2;

TEST_CASE("Loaded Trimesh morph edits undo to their authored value",
        "[cycle-v2][trimesh][morph][serialization][undo]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File graphFile = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("resources")
            .getChildFile("with-spies.cyclegraph");
    GraphLoadResult loaded = GraphSerializer().loadJsonString(
            graphFile.loadFileAsString());
    REQUIRE(loaded.succeeded());

    GraphDocument document(std::move(loaded.graph));
    GraphCommandDispatcher commands(document);
    const Node* loadedMesh = document.graph().findNode("waveMesh");
    REQUIRE(loadedMesh != nullptr);
    REQUIRE(NodeParameterMap(*loadedMesh)
            .floatValue("yellow") == Catch::Approx(0.317f));

    commands.beginTransientEdit();
    REQUIRE(commands.setNodeParameter(
            "waveMesh", "yellow", "Yellow", "0.55").succeeded());
    REQUIRE(commands.setNodeParameter(
            "waveMesh", "yellow", "Yellow", "0.82").succeeded());
    commands.commitTransientEdit();

    REQUIRE(NodeParameterMap(*document.graph().findNode("waveMesh"))
            .floatValue("yellow") == Catch::Approx(0.82f));
    REQUIRE(document.undo());
    REQUIRE(NodeParameterMap(*document.graph().findNode("waveMesh"))
            .floatValue("yellow") == Catch::Approx(0.317f));
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

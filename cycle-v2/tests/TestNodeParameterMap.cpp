#include <catch2/catch_test_macros.hpp>

#include "Graph/NodeParameterMap.h"

using namespace CycleV2;

TEST_CASE("Node parameter map provides typed keyed lookup",
        "[cycle-v2][graph][parameters]") {
    const std::vector<NodeParameter> parameters {
        { "float", "Float", "0.375" },
        { "integer", "Integer", "7" },
        { "boolean", "Boolean", "true" },
        { "choice", "Choice", "additive" }
    };
    const NodeParameterMap parameterMap(parameters);

    REQUIRE(parameterMap.floatValue("float") == 0.375f);
    REQUIRE(parameterMap.intValue("integer") == 7);
    REQUIRE(parameterMap.boolValue("boolean"));
    REQUIRE(parameterMap.stringValue("choice") == "additive");
    REQUIRE(parameterMap.floatValue("missing", 0.5f) == 0.5f);
}

TEST_CASE("Node parameter map preserves the first duplicate value",
        "[cycle-v2][graph][parameters]") {
    const std::vector<NodeParameter> parameters {
        { "value", "Value", "0.25" },
        { "value", "Duplicate", "0.75" }
    };

    REQUIRE(NodeParameterMap(parameters).floatValue("value") == 0.25f);
}

#include <catch2/catch_test_macros.hpp>

#include "App/CycleV2AutomationAssertions.h"
#include "App/CycleV2AutomationProtocol.h"

using namespace CycleV2;
using namespace juce;

namespace {

var commandWith(const String& command, const String& property, var value) {
    var result = AutomationProtocol::makeObject();
    auto* object = AutomationProtocol::objectFor(result);
    object->setProperty("command", command);
    object->setProperty(property, std::move(value));
    return result;
}

}

TEST_CASE("Automation assertions evaluate state and node parameters",
        "[cycle-v2][automation][assertions]") {
    var snapshot = AutomationProtocol::makeObject();
    AutomationProtocol::objectFor(snapshot)->setProperty("revision", 7);
    CycleV2AutomationAssertions assertions(
            [snapshot]() { return snapshot; },
            [](const String& nodeId, const String& parameterId, String& value) {
                if (nodeId != "node" || parameterId != "gain") {
                    return false;
                }
                value = "0.5";
                return true;
            });

    var stateCommand = commandWith("assertState", "path", "revision");
    auto* stateObject = AutomationProtocol::objectFor(stateCommand);
    stateObject->setProperty("op", "greaterThan");
    stateObject->setProperty("value", 6);
    REQUIRE((bool) AutomationProtocol::objectFor(
            assertions.assertState(stateCommand))->getProperty("ok"));

    var parameterCommand = commandWith("assertNodeParameter", "nodeId", "node");
    auto* parameterObject = AutomationProtocol::objectFor(parameterCommand);
    parameterObject->setProperty("parameterId", "gain");
    parameterObject->setProperty("equals", "0.5");
    REQUIRE((bool) AutomationProtocol::objectFor(
            assertions.assertNodeParameter(parameterCommand))->getProperty("ok"));

    const var paths = assertions.listAssertionPaths();
    REQUIRE((bool) AutomationProtocol::objectFor(paths)->getProperty("ok"));
}

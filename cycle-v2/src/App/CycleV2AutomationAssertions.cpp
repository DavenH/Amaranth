#include "App/CycleV2AutomationAssertions.h"

#include "App/CycleV2AutomationProtocol.h"

namespace CycleV2 {

using namespace juce;
using namespace AutomationProtocol;

CycleV2AutomationAssertions::CycleV2AutomationAssertions(
        SnapshotProvider snapshotProvider,
        ParameterReader parameterReader) :
        snapshot(std::move(snapshotProvider))
    ,   readParameter(std::move(parameterReader)) {
}

var CycleV2AutomationAssertions::assertState(const var& command) const {
    const String path = stringProperty(command, "path");
    const auto* commandObject = objectFor(command);
    const var equalsValue = commandObject == nullptr
            ? var()
            : commandObject->getProperty("equals");
    const var value = commandObject == nullptr
            ? var()
            : commandObject->getProperty("value");
    const String op = stringProperty(
            command, "op", equalsValue.isVoid() ? "exists" : "equals");
    const var expected = !equalsValue.isVoid() ? equalsValue : value;

    var actual;
    if (!getPathValue(snapshot(), path, actual)) {
        return failedResult("assertState", "State path not found: " + path);
    }
    if (!compareValues(actual, op, expected)) {
        var data = makeObject();
        auto* object = objectFor(data);
        object->setProperty("path", path);
        object->setProperty("op", op);
        object->setProperty("expected", expected);
        object->setProperty("actual", actual);
        var result = failedResult("assertState", "State assertion failed: " + path);
        objectFor(result)->setProperty("data", data);
        return result;
    }

    var data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("path", path);
    object->setProperty("op", op);
    object->setProperty("actual", actual);
    return okResult("assertState", data);
}

var CycleV2AutomationAssertions::assertNodeParameter(const var& command) const {
    const String nodeId = stringProperty(command, "nodeId");
    const String parameterId = stringProperty(command, "parameterId");
    const auto* commandObject = objectFor(command);
    const var equalsValue = commandObject == nullptr
            ? var()
            : commandObject->getProperty("equals");
    const var value = commandObject == nullptr
            ? var()
            : commandObject->getProperty("value");
    const String op = stringProperty(command, "op", "equals");
    const var expected = !equalsValue.isVoid() ? equalsValue : value;
    String actualString;

    if (nodeId.isEmpty() || parameterId.isEmpty()) {
        return failedResult("assertNodeParameter", "Missing nodeId or parameterId");
    }
    if (!readParameter(nodeId, parameterId, actualString)) {
        return failedResult(
                "assertNodeParameter",
                "Node parameter not found: " + nodeId + "." + parameterId);
    }

    const var actual = actualString;
    if (!compareValues(actual, op, expected)) {
        var data = makeObject();
        auto* object = objectFor(data);
        object->setProperty("nodeId", nodeId);
        object->setProperty("parameterId", parameterId);
        object->setProperty("op", op);
        object->setProperty("expected", expected);
        object->setProperty("actual", actual);
        var result = failedResult(
                "assertNodeParameter",
                "Node parameter assertion failed: " + nodeId + "." + parameterId);
        objectFor(result)->setProperty("data", data);
        return result;
    }

    var data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("nodeId", nodeId);
    object->setProperty("parameterId", parameterId);
    object->setProperty("op", op);
    object->setProperty("actual", actual);
    return okResult("assertNodeParameter", data);
}

var CycleV2AutomationAssertions::listAssertionPaths() const {
    Array<var> paths;
    flattenPaths(snapshot(), {}, paths);
    var data = makeObject();
    objectFor(data)->setProperty("paths", paths);
    return okResult("listAssertionPaths", data);
}

}

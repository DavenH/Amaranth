#include "App/CycleV2AutomationProtocol.h"

#include <cmath>

namespace CycleV2::AutomationProtocol {

using namespace juce;

var makeObject() {
    return new DynamicObject();
}

DynamicObject* objectFor(var& value) {
    return value.getDynamicObject();
}

const DynamicObject* objectFor(const var& value) {
    return value.getDynamicObject();
}

String stringProperty(
        const var& value,
        const Identifier& property,
        const String& fallback) {
    if (const auto* object = objectFor(value)) {
        const var found = object->getProperty(property);
        return found.isVoid() ? fallback : found.toString();
    }
    return fallback;
}

bool boolProperty(const var& value, const Identifier& property, bool fallback) {
    if (const auto* object = objectFor(value)) {
        const var found = object->getProperty(property);
        return found.isVoid() ? fallback : (bool) found;
    }
    return fallback;
}

int intProperty(const var& value, const Identifier& property, int fallback) {
    if (const auto* object = objectFor(value)) {
        const var found = object->getProperty(property);
        return found.isVoid() ? fallback : (int) found;
    }
    return fallback;
}

float floatProperty(const var& value, const Identifier& property, float fallback) {
    if (const auto* object = objectFor(value)) {
        const var found = object->getProperty(property);
        return found.isVoid() ? fallback : (float) (double) found;
    }
    return fallback;
}

var okResult(const String& type, var data) {
    var result = makeObject();
    auto* object = objectFor(result);
    object->setProperty("ok", true);
    object->setProperty("type", type);
    if (!data.isVoid()) {
        object->setProperty("data", data);
    }
    return result;
}

var failedResult(const String& type, const String& message) {
    var result = makeObject();
    auto* object = objectFor(result);
    object->setProperty("ok", false);
    object->setProperty("type", type);
    object->setProperty("message", message);
    return result;
}

bool getPathValue(const var& root, const String& path, var& result) {
    if (path.isEmpty()) {
        result = root;
        return true;
    }

    var current = root;
    StringArray parts;
    parts.addTokens(path, ".", {});
    for (const auto& part : parts) {
        if (auto* array = current.getArray()) {
            const int index = part.getIntValue();
            if (index < 0 || index >= array->size()) {
                return false;
            }
            current = array->getReference(index);
            continue;
        }

        const auto* object = objectFor(current);
        if (object == nullptr) {
            return false;
        }
        current = object->getProperty(Identifier(part));
        if (current.isVoid()) {
            return false;
        }
    }
    result = current;
    return true;
}

void flattenPaths(const var& value, const String& prefix, Array<var>& paths) {
    if (const auto* object = objectFor(value)) {
        const NamedValueSet& properties = object->getProperties();
        for (int i = 0; i < properties.size(); ++i) {
            const String propertyName = properties.getName(i).toString();
            const String next = prefix.isEmpty()
                    ? propertyName
                    : prefix + "." + propertyName;
            flattenPaths(properties.getValueAt(i), next, paths);
        }
        return;
    }
    if (const auto* array = value.getArray()) {
        for (int i = 0; i < array->size(); ++i) {
            const String next = prefix.isEmpty()
                    ? String(i)
                    : prefix + "." + String(i);
            flattenPaths(array->getReference(i), next, paths);
        }
        return;
    }

    var entry = makeObject();
    auto* object = objectFor(entry);
    object->setProperty("path", prefix);
    object->setProperty("value", value);
    object->setProperty(
            "type",
            value.isBool()
                    ? "bool"
                    : value.isDouble() || value.isInt() ? "number" : "string");
    paths.add(entry);
}

bool compareValues(const var& actual, const String& op, const var& expected) {
    const auto valuesMatch = [&]() {
        if (actual.isDouble()
                || actual.isInt()
                || expected.isDouble()
                || expected.isInt()) {
            return std::abs((double) actual - (double) expected) < 0.000001;
        }
        return actual == expected;
    };
    if (op == "exists") {
        return true;
    }
    if (op == "equals") {
        return valuesMatch();
    }
    if (op == "notEquals") {
        return !valuesMatch();
    }

    const double actualNumber = (double) actual;
    const double expectedNumber = (double) expected;
    if (op == "lessThan") {
        return actualNumber < expectedNumber;
    }
    if (op == "lessThanOrEqual") {
        return actualNumber <= expectedNumber;
    }
    if (op == "greaterThan") {
        return actualNumber > expectedNumber;
    }
    return op == "greaterThanOrEqual" && actualNumber >= expectedNumber;
}

}

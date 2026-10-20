#include "Nodes/ModelAuthoringValidation.h"

#include <cmath>
#include <limits>
#include <unordered_set>

namespace CycleV2::ModelAuthoringValidation {

namespace {

bool number(const juce::var& value, double minimum, double maximum) {
    return (value.isInt() || value.isInt64() || value.isDouble())
            && std::isfinite((double) value)
            && (double) value >= minimum && (double) value <= maximum;
}

bool integer(const juce::var& value, double minimum = 0,
        double maximum = std::numeric_limits<int>::max()) {
    return number(value, minimum, maximum) && std::floor((double) value) == (double) value;
}

bool revision(const juce::var& value) {
    return (value.isInt() || value.isInt64()) && (juce::int64) value > 0;
}

bool optionalSelection(const juce::var& value) {
    if (!value.hasProperty("selection")) {
        return true;
    }
    const auto selection = value["selection"];
    return (selection.isInt() || selection.isInt64()) && (juce::int64) selection >= -1;
}

bool unitFields(const juce::var& value, std::initializer_list<const char*> fields) {
    if (value.getDynamicObject() == nullptr) {
        return false;
    }
    for (const char* field : fields) {
        if (!number(value[field], 0, 1)) {
            return false;
        }
    }
    return true;
}

bool indices(const juce::var& value, int count) {
    const auto* array = value.getArray();
    if (array == nullptr) {
        return false;
    }
    std::unordered_set<int> seen;
    for (const auto& index : *array) {
        if (!integer(index, 0, count - 1) || !seen.insert((int) index).second) {
            return false;
        }
    }
    return true;
}

}

bool envelope(const juce::var& value, const juce::String& schema, int version) {
    return value.getDynamicObject() != nullptr && value["schema"].isString()
            && value["schema"].toString() == schema
            && integer(value["version"], version, version) && revision(value["revision"]);
}

bool mesh(const juce::var& value) {
    const auto* vertices = value["vertices"].getArray();
    const auto* cubes = value["cubes"].getArray();
    if (value.getDynamicObject() == nullptr || vertices == nullptr || cubes == nullptr
            || vertices->size() > maximumVertices || cubes->size() > maximumCubes) {
        return false;
    }
    if ((value.hasProperty("name") && !value["name"].isString())
            || (value.hasProperty("version") && !integer(value["version"], 1))) {
        return false;
    }
    std::unordered_set<int> identities;
    for (const auto& vertex : *vertices) {
        // Legacy geometry includes wraparound phase and overshooting time/amplitude.
        // Preserve those coordinates while preventing overflow in Mesh's float storage.
        constexpr double coordinateLimit = std::numeric_limits<float>::max();
        if (!unitFields(vertex, { "key", "mod", "weight" })
                || !number(vertex["time"], -coordinateLimit, coordinateLimit)
                || !number(vertex["phase"], -coordinateLimit, coordinateLimit)
                || !number(vertex["amp"], -coordinateLimit, coordinateLimit)
                || !integer(vertex["id"]) || !identities.insert((int) vertex["id"]).second) {
            return false;
        }
    }
    for (const auto& cube : *cubes) {
        const auto* ids = cube["vertexIds"].getArray();
        if (cube.getDynamicObject() == nullptr || ids == nullptr || ids->size() != 8) {
            return false;
        }
        std::unordered_set<int> used;
        for (const auto& id : *ids) {
            if (!integer(id) || identities.count((int) id) == 0
                    || !used.insert((int) id).second) {
                return false;
            }
        }
        // These legacy fields are optional; supplied fields must be well typed.
        const auto guides = cube["guides"];
        const auto gains = cube["gains"];
        if ((cube.hasProperty("guides") && guides.getDynamicObject() == nullptr)
                || (cube.hasProperty("gains") && gains.getDynamicObject() == nullptr)) {
            return false;
        }
        for (const char* field : { "time", "key", "mod", "phase", "amp", "curve" }) {
            if ((guides.hasProperty(field)
                        && !integer(guides[field], -1, std::numeric_limits<signed char>::max()))
                    || (gains.hasProperty(field) && !number(gains[field], 0, 1))) {
                return false;
            }
        }
    }
    return true;
}

bool curve(const juce::var& value, bool isEnvelope) {
    if (value.getDynamicObject() == nullptr || !revision(value["revision"])
            || !value["type"].isString()
            || value["type"].toString() != (isEnvelope ? "envelope" : "flatCurve")
            || !optionalSelection(value)
            || !integer(value["version"], isEnvelope ? 2 : 1, isEnvelope ? 2 : 1)) {
        return false;
    }
    if (isEnvelope) {
        const auto meshValue = value["mesh"];
        const auto* cubes = meshValue["mainMesh"]["cubes"].getArray();
        const auto* ids = value["cubeIds"].getArray();
        if (meshValue.getDynamicObject() == nullptr
                || (meshValue.hasProperty("name") && !meshValue["name"].isString())
                || !mesh(meshValue["mainMesh"]) || cubes == nullptr || ids == nullptr
                || ids->isEmpty() || ids->size() != cubes->size()
                || !indices(meshValue["loopIndices"], cubes->size())
                || !indices(meshValue["sustainIndices"], cubes->size())
                || !value["logarithmic"].isBool() || !unitFields(value, { "red", "blue" })) {
            return false;
        }
        std::unordered_set<juce::int64> unique;
        for (const auto& id : *ids) {
            if (!revision(id) || !unique.insert((juce::int64) id).second) {
                return false;
            }
        }
        return true;
    }
    const auto* vertices = value["vertices"].getArray();
    if (vertices == nullptr || vertices->size() < 2 || vertices->size() > maximumVertices) {
        return false;
    }
    std::unordered_set<juce::int64> unique;
    for (const auto& vertex : *vertices) {
        if (!unitFields(vertex, { "x", "y", "curve" }) || !revision(vertex["id"])
                || !unique.insert((juce::int64) vertex["id"]).second) {
            return false;
        }
    }
    return true;
}

}

#include "TrimeshMeshEditState.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>

namespace CycleV2 {

bool TrimeshVertexEdit::operator==(const TrimeshVertexEdit& other) const {
    return vertexIndex == other.vertexIndex
        && valueIndex == other.valueIndex
        && value == other.value
        && sourceId == other.sourceId;
}

TrimeshMeshEditState TrimeshMeshEditState::fromNode(const Node& node) {
    return fromParameters(node.parameters);
}

TrimeshMeshEditState TrimeshMeshEditState::fromParameters(
        const std::vector<NodeParameter>& parameters) {
    TrimeshMeshEditState state;

    for (const auto& parameter : parameters) {
        TrimeshVertexEdit edit;

        if (parseVertexEditParameter(parameter, edit)) {
            state.vertexEdits.push_back(edit);
        }
    }

    return state;
}

TrimeshMeshEditState TrimeshMeshEditState::fromMesh(Mesh& mesh) {
    TrimeshMeshEditState state;
    const auto& verts = mesh.getVerts();
    constexpr int fields[] {
            Vertex::Time,
            Vertex::Red,
            Vertex::Blue,
            Vertex::Phase,
            Vertex::Amp,
            Vertex::Curve
    };
    constexpr size_t fieldCount = sizeof(fields) / sizeof(fields[0]);

    state.vertexEdits.reserve(verts.size() * fieldCount);

    for (int vertexIndex = 0; vertexIndex < (int) verts.size(); ++vertexIndex) {
        const Vertex* vertex = verts[(size_t) vertexIndex];

        if (vertex == nullptr) {
            continue;
        }

        for (const int valueIndex : fields) {
            const String field = fieldForVertexValueIndex(valueIndex);

            state.vertexEdits.push_back({
                    vertexIndex,
                    valueIndex,
                    vertex->values[valueIndex],
                    canonicalVertexParameterId(vertexIndex, field)
            });
        }
    }

    return state;
}

String TrimeshMeshEditState::canonicalVertexParameterId(int vertexIndex, const String& field) {
    return "mesh.vertex." + String(vertexIndex) + "." + field;
}

String TrimeshMeshEditState::fieldForVertexValueIndex(int valueIndex) {
    switch (valueIndex) {
        case Vertex::Amp:      return "amp";
        case Vertex::Phase:    return "phase";
        case Vertex::Time:     return "time";
        case Vertex::Red:      return "red";
        case Vertex::Blue:     return "blue";
        case Vertex::Curve:    return "curve";
        default:               return {};
    }
}

bool TrimeshMeshEditState::applyTo(Mesh& mesh) const {
    auto& verts = mesh.getVerts();
    bool changed {};

    for (const TrimeshVertexEdit& edit : vertexEdits) {
        if (!isPositiveAndBelow(edit.vertexIndex, (int) verts.size())
                || verts[(size_t) edit.vertexIndex] == nullptr) {
            continue;
        }

        Vertex* vertex = verts[(size_t) edit.vertexIndex];
        const float clampedValue = jlimit(0.f, 1.f, edit.value);

        if (vertex->values[edit.valueIndex] == clampedValue) {
            continue;
        }

        vertex->values[edit.valueIndex] = clampedValue;
        changed = true;
    }

    return changed;
}

bool TrimeshMeshEditState::operator==(const TrimeshMeshEditState& other) const {
    return vertexEdits == other.vertexEdits;
}

bool TrimeshMeshEditState::parseVertexEditParameter(
        const NodeParameter& parameter,
        TrimeshVertexEdit& edit) {
    String suffix;

    if (parameter.id.startsWith("mesh.vertex.")) {
        suffix = parameter.id.fromFirstOccurrenceOf("mesh.vertex.", false, false);
    } else if (parameter.id.startsWith("vertex.")) {
        suffix = parameter.id.fromFirstOccurrenceOf("vertex.", false, false);
    } else {
        return false;
    }

    const String indexText = suffix.upToFirstOccurrenceOf(".", false, false);
    const String field = suffix.fromFirstOccurrenceOf(".", false, false);

    if (indexText.isEmpty() || !indexText.containsOnly("0123456789")) {
        return false;
    }

    int valueIndex {};

    if (!fieldToVertexValueIndex(field, valueIndex)) {
        return false;
    }

    edit.vertexIndex = indexText.getIntValue();
    edit.valueIndex = valueIndex;
    edit.value = parameter.value.getFloatValue();
    edit.sourceId = parameter.id;
    return true;
}

bool TrimeshMeshEditState::fieldToVertexValueIndex(const String& field, int& valueIndex) {
    if (field == "amp") {
        valueIndex = Vertex::Amp;
    } else if (field == "phase") {
        valueIndex = Vertex::Phase;
    } else if (field == "time") {
        valueIndex = Vertex::Time;
    } else if (field == "red") {
        valueIndex = Vertex::Red;
    } else if (field == "blue") {
        valueIndex = Vertex::Blue;
    } else if (field == "curve") {
        valueIndex = Vertex::Curve;
    } else {
        return false;
    }

    return true;
}

}

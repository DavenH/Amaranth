#include "Nodes/Trimesh/Model/TrimeshVertexEditCore.h"

#include "Graph/InteractionComplexityDiagnostics.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Mesh/VertCube.h>

#include <utility>

namespace CycleV2 {

namespace {

const Vertex* vertexAt(const Mesh& mesh, int index) {
    const auto& vertices = mesh.getVerts();
    return isPositiveAndBelow(index, (int) vertices.size())
            ? vertices[(size_t) index]
            : nullptr;
}

Vertex* vertexAt(Mesh& mesh, int index) {
    return const_cast<Vertex*>(vertexAt(static_cast<const Mesh&>(mesh), index));
}

float averageGuideGain(const Vertex& vertex, int valueIndex) {
    float sum {};
    int count {};
    for (const auto* owner : vertex.owners) {
        InteractionComplexityDiagnostics::recordMeshEditOwnerVisit();
        if (owner != nullptr) {
            sum += owner->guideCurveGainAt(valueIndex);
            ++count;
        }
    }
    return count > 0 ? sum / (float) count : 0.5f;
}

}

TrimeshVertexEditDelta TrimeshVertexEditDelta::inverse() const {
    TrimeshVertexEditDelta result = *this;
    for (auto& change : result.changes) {
        std::swap(change.before, change.after);
    }
    return result;
}

std::optional<TrimeshVertexEditDelta> TrimeshVertexEditCore::prepareVertexValue(
        const Mesh& mesh,
        int vertexIndex,
        const juce::String& parameterId,
        float value) {
    const Vertex* vertex = vertexAt(mesh, vertexIndex);
    const int element = valueIndex(parameterId);
    if (vertex == nullptr || element < 0) {
        return std::nullopt;
    }

    TrimeshVertexEditDelta delta;
    delta.vertexIndex = vertexIndex;
    delta.valueIndex = element;
    const float target = jlimit(0.f, 1.f, value);
    if (vertex->values[element] != target) {
        delta.changes.push_back({ -1, vertex->values[element], target });
    }
    return delta;
}

std::optional<TrimeshVertexEditDelta> TrimeshVertexEditCore::prepareGuideGain(
        const Mesh& mesh,
        int vertexIndex,
        const juce::String& parameterId,
        float value) {
    const Vertex* vertex = vertexAt(mesh, vertexIndex);
    const int element = valueIndex(parameterId);
    if (vertex == nullptr || element < 0 || vertex->owners.isEmpty()) {
        return std::nullopt;
    }

    TrimeshVertexEditDelta delta;
    delta.target = TrimeshVertexEditTarget::GuideGain;
    delta.vertexIndex = vertexIndex;
    delta.valueIndex = element;
    const float offset = jlimit(0.f, 1.f, value)
            - averageGuideGain(*vertex, element);
    for (int ordinal = 0; ordinal < vertex->owners.size(); ++ordinal) {
        InteractionComplexityDiagnostics::recordMeshEditOwnerVisit();
        const auto* owner = vertex->owners[ordinal];
        if (owner == nullptr) {
            continue;
        }
        const float before = owner->guideCurveGainAt(element);
        const float after = jlimit(0.f, 1.f, before + offset);
        if (before != after) {
            delta.changes.push_back({ ordinal, before, after });
        }
    }
    return delta;
}

bool TrimeshVertexEditCore::apply(
        Mesh& mesh,
        const TrimeshVertexEditDelta& delta) {
    if (!canApply(mesh, delta)) {
        return false;
    }
    Vertex* vertex = vertexAt(mesh, delta.vertexIndex);
    for (const auto& change : delta.changes) {
        if (delta.target == TrimeshVertexEditTarget::VertexValue) {
            vertex->values[delta.valueIndex] = change.after;
        } else {
            InteractionComplexityDiagnostics::recordMeshEditOwnerVisit();
            vertex->owners[change.ownerOrdinal]
                    ->guideCurveGainAt(delta.valueIndex) = change.after;
        }
    }
    return true;
}

bool TrimeshVertexEditCore::canApply(
        const Mesh& mesh,
        const TrimeshVertexEditDelta& delta) {
    const Vertex* vertex = vertexAt(mesh, delta.vertexIndex);
    if (vertex == nullptr || !isPositiveAndBelow(delta.valueIndex, Vertex::numElements)) {
        return false;
    }
    if (delta.target == TrimeshVertexEditTarget::VertexValue
            && delta.changes.size() > 1) {
        return false;
    }

    int previousOrdinal = -1;
    for (const auto& change : delta.changes) {
        if (delta.target == TrimeshVertexEditTarget::VertexValue) {
            if (change.ownerOrdinal != -1
                    || vertex->values[delta.valueIndex] != change.before) {
                return false;
            }
            continue;
        }
        if (!isPositiveAndBelow(change.ownerOrdinal, vertex->owners.size())) {
            return false;
        }
        if (change.ownerOrdinal <= previousOrdinal) {
            return false;
        }
        previousOrdinal = change.ownerOrdinal;
        InteractionComplexityDiagnostics::recordMeshEditOwnerVisit();
        const auto* owner = vertex->owners[change.ownerOrdinal];
        if (owner == nullptr
                || owner->guideCurveGainAt(delta.valueIndex) != change.before) {
            return false;
        }
    }

    return true;
}

std::optional<TrimeshVertexEditDelta> TrimeshVertexEditCore::compose(
        const TrimeshVertexEditDelta& accumulated,
        const TrimeshVertexEditDelta& movement) {
    if (accumulated.target != movement.target
            || accumulated.vertexIndex != movement.vertexIndex
            || accumulated.valueIndex != movement.valueIndex) {
        return std::nullopt;
    }

    TrimeshVertexEditDelta result;
    result.target = accumulated.target;
    result.vertexIndex = accumulated.vertexIndex;
    result.valueIndex = accumulated.valueIndex;
    result.changes.reserve(accumulated.changes.size() + movement.changes.size());
    size_t accumulatedIndex {};
    size_t movementIndex {};
    while (accumulatedIndex < accumulated.changes.size()
            || movementIndex < movement.changes.size()) {
        if (movementIndex == movement.changes.size()
                || (accumulatedIndex < accumulated.changes.size()
                        && accumulated.changes[accumulatedIndex].ownerOrdinal
                                < movement.changes[movementIndex].ownerOrdinal)) {
            result.changes.push_back(accumulated.changes[accumulatedIndex++]);
            continue;
        }
        if (accumulatedIndex == accumulated.changes.size()
                || movement.changes[movementIndex].ownerOrdinal
                        < accumulated.changes[accumulatedIndex].ownerOrdinal) {
            result.changes.push_back(movement.changes[movementIndex++]);
            continue;
        }
        const auto& previous = accumulated.changes[accumulatedIndex++];
        const auto& next = movement.changes[movementIndex++];
        if (previous.after != next.before) {
            return std::nullopt;
        }
        if (previous.before != next.after) {
            result.changes.push_back({ previous.ownerOrdinal, previous.before, next.after });
        }
    }
    return result;
}

float TrimeshVertexEditCore::guideGain(
        const Mesh& mesh,
        int vertexIndex,
        const juce::String& parameterId) {
    const Vertex* vertex = vertexAt(mesh, vertexIndex);
    const int element = valueIndex(parameterId);
    return vertex != nullptr && element >= 0
            ? averageGuideGain(*vertex, element)
            : 0.5f;
}

int TrimeshVertexEditCore::valueIndex(const juce::String& parameterId) {
    const juce::String field = parameterId.fromLastOccurrenceOf(".", false, false);
    if (field == "time") {
        return Vertex::Time;
    }
    if (field == "red") {
        return Vertex::Red;
    }
    if (field == "blue") {
        return Vertex::Blue;
    }
    if (field == "phase") {
        return Vertex::Phase;
    }
    if (field == "amp") {
        return Vertex::Amp;
    }
    if (field == "curve") {
        return Vertex::Curve;
    }
    return -1;
}

}

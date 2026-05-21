#include "TrimeshNodeModel.h"

#include "TrimeshBlockwiseDsp.h"
#include "TrimeshGridwiseDsp.h"
#include "TrimeshMeshFactory.h"

#include <Array/Buffer.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Mesh/VertCube.h>

namespace CycleV2 {

namespace {

float parameterFloat(const Node& node, const String& id, float fallback) {
    for (const auto& parameter : node.parameters) {
        if (parameter.id == id) {
            return parameter.value.getFloatValue();
        }
    }

    return fallback;
}

bool parameterFloatValue(const Node& node, const String& id, float& value) {
    for (const auto& parameter : node.parameters) {
        if (parameter.id == id) {
            value = parameter.value.getFloatValue();
            return true;
        }
    }

    return false;
}

String parameterString(const Node& node, const String& id, const String& fallback) {
    for (const auto& parameter : node.parameters) {
        if (parameter.id == id) {
            return parameter.value;
        }
    }

    return fallback;
}

int parameterInt(const Node& node, const String& id, int fallback) {
    for (const auto& parameter : node.parameters) {
        if (parameter.id == id) {
            return parameter.value.getIntValue();
        }
    }

    return fallback;
}

int primaryAxisFromParameter(const String& axisName) {
    if (axisName == "red") {
        return Vertex::Red;
    }

    if (axisName == "blue") {
        return Vertex::Blue;
    }

    return Vertex::Time;
}

void normalizeBipolarBlock(AudioProcessBlock& block) {
    if (block.samples.empty()) {
        return;
    }

    Buffer<float>(block.samples.data(), (int) block.samples.size())
            .mul(0.5f)
            .add(0.5f)
            .clip(0.f, 1.f);
}

}

TrimeshNodeModel::TrimeshNodeModel() :
        primaryViewAxis(Vertex::Time) {}

TrimeshNodeModel::~TrimeshNodeModel() {
    clearMesh();
}

TrimeshNodeModel::TrimeshNodeModel(TrimeshNodeModel&& other) noexcept :
        ownedMesh       (std::move(other.ownedMesh))
    ,   morph           (other.morph)
    ,   primaryViewAxis (other.primaryViewAxis)
    ,   selectedVertexIndex (other.selectedVertexIndex)
    ,   revision        (other.revision) {}

TrimeshNodeModel& TrimeshNodeModel::operator=(TrimeshNodeModel&& other) noexcept {
    if (this != &other) {
        clearMesh();
        ownedMesh = std::move(other.ownedMesh);
        morph = other.morph;
        primaryViewAxis = other.primaryViewAxis;
        selectedVertexIndex = other.selectedVertexIndex;
        revision = other.revision;
    }

    return *this;
}

void TrimeshNodeModel::syncFromNode(const Node& node) {
    const MorphPosition nextMorph {
            parameterFloat(node, "yellow", 0.5f),
            parameterFloat(node, "red", 0.5f),
            parameterFloat(node, "blue", 0.5f)
    };
    const int nextPrimaryAxis = primaryAxisFromParameter(
            parameterString(node, "primaryAxis", "yellow"));
    const int nextSelectedVertexIndex = parameterInt(node, "selectedVertexIndex", -1);

    if (nextMorph.time.getTargetValue() != morph.time.getTargetValue()
            || nextMorph.red.getTargetValue() != morph.red.getTargetValue()
            || nextMorph.blue.getTargetValue() != morph.blue.getTargetValue()
            || nextPrimaryAxis != primaryViewAxis
            || nextSelectedVertexIndex != selectedVertexIndex) {
        morph = nextMorph;
        primaryViewAxis = nextPrimaryAxis;
        selectedVertexIndex = nextSelectedVertexIndex;
        ++revision;
    }

    if (applyVertexParameterOverrides(node)) {
        ++revision;
    }
}

TrimeshRenderData TrimeshNodeModel::renderGrid(int rows, int columns) {
    rows = jmax(2, rows);
    columns = jmax(2, columns);

    TrimeshRenderData result;
    result.rows = rows;
    result.columns = columns;

    TrimeshBlockwiseDsp blockwiseDsp;
    AudioProcessBlock slice;
    blockwiseDsp.setMesh(&mesh());
    blockwiseDsp.setMorphPosition(morph);
    blockwiseDsp.setPrimaryViewAxis(primaryViewAxis);
    blockwiseDsp.renderCycle((size_t) rows, PortDomain::TimeSignal, ChannelLayout::LinkedStereo, slice);
    normalizeBipolarBlock(slice);
    result.slice = std::move(slice.samples);

    TrimeshGridwiseDsp gridwiseDsp;
    const auto gridColumns = gridwiseDsp.renderColumns(
            mesh(),
            morph,
            primaryViewAxis,
            (size_t) columns,
            (size_t) rows,
            PortDomain::TimeSignal,
            ChannelLayout::LinkedStereo);

    result.surface.reserve((size_t) rows * (size_t) columns);

    for (auto column : gridColumns) {
        normalizeBipolarBlock(column.signal);
        result.surface.insert(
                result.surface.end(),
                column.signal.samples.begin(),
                column.signal.samples.end());
    }

    return result;
}

std::vector<TrimeshVertexParameter> TrimeshNodeModel::getSelectedVertexParameters() {
    Vertex* selectedVertex = this->selectedVertex();

    if (selectedVertex == nullptr) {
        return {};
    }

    return {
            { "vertex.amp", "Amplitude", selectedVertex->values[Vertex::Amp], 0.f, 1.f },
            { "vertex.phase", "Phase", selectedVertex->values[Vertex::Phase], 0.f, 1.f },
            { "vertex.curve", "Sharpness", selectedVertex->values[Vertex::Curve], 0.f, 1.f }
    };
}

std::vector<TrimeshVertexMarker> TrimeshNodeModel::getVertexMarkers() {
    Mesh& activeMesh = mesh();
    std::vector<TrimeshVertexMarker> markers;
    const auto& verts = activeMesh.getVerts();

    markers.reserve(verts.size());

    for (int i = 0; i < (int) verts.size(); ++i) {
        const Vertex* vertex = verts[(size_t) i];

        if (vertex == nullptr) {
            continue;
        }

        markers.push_back({
                i,
                jlimit(0.f, 1.f, vertex->values[Vertex::Phase]),
                jlimit(0.f, 1.f, vertex->values[Vertex::Amp]),
                i == selectedVertexIndex
        });
    }

    return markers;
}

int TrimeshNodeModel::findNearestVertexIndexForPhaseAmp(float phase, float amp) {
    Mesh& activeMesh = mesh();
    int bestIndex { -1 };
    float bestDistance {};
    const float clampedPhase = jlimit(0.f, 1.f, phase);
    const float clampedAmp = jlimit(0.f, 1.f, amp);
    const auto& verts = activeMesh.getVerts();

    for (int i = 0; i < (int) verts.size(); ++i) {
        const Vertex* vertex = verts[(size_t) i];

        if (vertex == nullptr) {
            continue;
        }

        const float phaseDiff = vertex->values[Vertex::Phase] - clampedPhase;
        const float ampDiff = vertex->values[Vertex::Amp] - clampedAmp;
        const float distance = phaseDiff * phaseDiff + ampDiff * ampDiff;

        if (bestIndex < 0 || distance < bestDistance) {
            bestIndex = i;
            bestDistance = distance;
        }
    }

    return bestIndex;
}

Mesh& TrimeshNodeModel::mesh() {
    if (ownedMesh == nullptr) {
        ownedMesh = TrimeshMeshFactory::createDefaultMesh("Cycle2TrimeshNode");
        ++revision;
    }

    return *ownedMesh;
}

Vertex* TrimeshNodeModel::selectedVertex() {
    Mesh& activeMesh = mesh();
    auto& verts = activeMesh.getVerts();

    if (isPositiveAndBelow(selectedVertexIndex, (int) verts.size())) {
        return verts[(size_t) selectedVertexIndex];
    }

    for (auto* cube : activeMesh.getCubes()) {
        if (cube == nullptr) {
            continue;
        }

        if (Vertex* vertex = cube->findClosestVertex(morph)) {
            return vertex;
        }
    }

    if (!activeMesh.getVerts().empty()) {
        return activeMesh.getVerts().front();
    }

    return nullptr;
}

bool TrimeshNodeModel::applyVertexParameterOverrides(const Node& node) {
    float amp {};
    float phase {};
    float curve {};
    const bool hasAmp = parameterFloatValue(node, "vertex.amp", amp);
    const bool hasPhase = parameterFloatValue(node, "vertex.phase", phase);
    const bool hasCurve = parameterFloatValue(node, "vertex.curve", curve);
    const int overrideIndex = parameterInt(node, "vertexOverrideIndex", selectedVertexIndex);

    if (!hasAmp && !hasPhase && !hasCurve) {
        return false;
    }

    if (overrideIndex != selectedVertexIndex) {
        return false;
    }

    Vertex* vertex = selectedVertex();

    if (vertex == nullptr) {
        return false;
    }

    bool changed {};
    auto apply = [&](int index, float value) {
        const float clampedValue = jlimit(0.f, 1.f, value);

        if (vertex->values[index] == clampedValue) {
            return;
        }

        vertex->values[index] = clampedValue;
        changed = true;
    };

    if (hasAmp) {
        apply(Vertex::Amp, amp);
    }

    if (hasPhase) {
        apply(Vertex::Phase, phase);
    }

    if (hasCurve) {
        apply(Vertex::Curve, curve);
    }

    return changed;
}

void TrimeshNodeModel::clearMesh() {
    if (ownedMesh != nullptr) {
        ownedMesh->destroy();
        ownedMesh = nullptr;
    }
}

}

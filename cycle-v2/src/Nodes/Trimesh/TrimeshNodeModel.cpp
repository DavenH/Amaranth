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
    ,   meshEditState   (std::move(other.meshEditState))
    ,   morph           (other.morph)
    ,   primaryViewAxis (other.primaryViewAxis)
    ,   selectedVertexIndex (other.selectedVertexIndex)
    ,   revision        (other.revision)
    ,   revisions       (other.revisions) {}

TrimeshNodeModel& TrimeshNodeModel::operator=(TrimeshNodeModel&& other) noexcept {
    if (this != &other) {
        clearMesh();
        ownedMesh = std::move(other.ownedMesh);
        meshEditState = std::move(other.meshEditState);
        morph = other.morph;
        primaryViewAxis = other.primaryViewAxis;
        selectedVertexIndex = other.selectedVertexIndex;
        revision = other.revision;
        revisions = other.revisions;
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
    const TrimeshMeshEditState nextMeshEditState = TrimeshMeshEditState::fromNode(node);

    if (nextMorph.time.getTargetValue() != morph.time.getTargetValue()
            || nextMorph.red.getTargetValue() != morph.red.getTargetValue()
            || nextMorph.blue.getTargetValue() != morph.blue.getTargetValue()) {
        morph = nextMorph;
        bumpMorphRevision();
    }

    if (nextPrimaryAxis != primaryViewAxis) {
        primaryViewAxis = nextPrimaryAxis;
        bumpPrimaryAxisRevision();
    }

    if (nextSelectedVertexIndex != selectedVertexIndex) {
        selectedVertexIndex = nextSelectedVertexIndex;
        bumpSelectedControlRevision();
    }

    if (applySerializedMeshEdits(nextMeshEditState)) {
        bumpMeshContentRevision();
    }

    if (nextMeshEditState.empty() && applyLegacySelectedVertexOverride(node)) {
        bumpMeshContentRevision();
    }
}

TrimeshRenderData TrimeshNodeModel::renderGrid(int rows, int columns, PortDomain domain) {
    rows = jmax(2, rows);
    columns = jmax(2, columns);
    const bool cyclic = domain == PortDomain::TimeSignal;

    TrimeshRenderData result;
    result.domain = domain;
    result.rows = rows;
    result.columns = columns;
    result.cyclic = cyclic;

    TrimeshBlockwiseDsp blockwiseDsp;
    AudioProcessBlock slice;
    blockwiseDsp.setMesh(&mesh());
    blockwiseDsp.setMorphPosition(morph);
    blockwiseDsp.setPrimaryViewAxis(primaryViewAxis);
    blockwiseDsp.setCyclic(cyclic);
    blockwiseDsp.renderCycle((size_t) rows, domain, ChannelLayout::LinkedStereo, slice);
    normalizeBipolarBlock(slice);
    result.slice = std::move(slice.samples);

    TrimeshGridwiseDsp gridwiseDsp;
    gridwiseDsp.setCyclic(cyclic);
    const auto gridColumns = gridwiseDsp.renderColumns(
            mesh(),
            morph,
            primaryViewAxis,
            (size_t) columns,
            (size_t) rows,
            domain,
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

std::vector<TrimeshVertexParameter> TrimeshNodeModel::getVertexParametersForIndex(int vertexIndex) {
    Vertex* selectedVertex = vertexAtIndex(vertexIndex);

    if (selectedVertex == nullptr) {
        return {};
    }

    return {
            { "vertex.time", "time", selectedVertex->values[Vertex::Time], 0.f, 1.f },
            { "vertex.red", "red", selectedVertex->values[Vertex::Red], 0.f, 1.f },
            { "vertex.blue", "blue", selectedVertex->values[Vertex::Blue], 0.f, 1.f },
            { "vertex.phase", "phase", selectedVertex->values[Vertex::Phase], 0.f, 1.f },
            { "vertex.amp", "amp", selectedVertex->values[Vertex::Amp], 0.f, 1.f },
            { "vertex.curve", "curve", selectedVertex->values[Vertex::Curve], 0.f, 1.f }
    };
}

std::vector<TrimeshVertexParameter> TrimeshNodeModel::getSelectedVertexParameters() {
    return getVertexParametersForIndex(resolvedSelectedVertexIndex());
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

std::vector<TrimeshCubePreviewVertex> TrimeshNodeModel::getSelectedCubePreviewVertices() {
    Mesh& activeMesh = mesh();
    Vertex* selected = selectedVertex();
    VertCube* previewCube = selected != nullptr && selected->owners.size() > 0
            ? selected->owners.getFirst()
            : nullptr;

    if (previewCube == nullptr) {
        for (auto* cube : activeMesh.getCubes()) {
            if (cube != nullptr && cube->findClosestVertex(morph) != nullptr) {
                previewCube = cube;
                break;
            }
        }
    }

    if (previewCube == nullptr) {
        return {};
    }

    std::vector<TrimeshCubePreviewVertex> vertices;
    vertices.reserve(VertCube::numVerts);

    for (int i = 0; i < (int) VertCube::numVerts; ++i) {
        Vertex* vertex = previewCube->getVertex(i);

        if (vertex == nullptr) {
            vertices.push_back({});
            continue;
        }

        vertices.push_back({
                jlimit(0.f, 1.f, vertex->values[Vertex::Time]),
                jlimit(0.f, 1.f, vertex->values[Vertex::Red]),
                jlimit(0.f, 1.f, vertex->values[Vertex::Blue]),
                vertex == selected
        });
    }

    return vertices;
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

int TrimeshNodeModel::getResolvedSelectedVertexIndex() {
    return resolvedSelectedVertexIndex();
}

void TrimeshNodeModel::markMeshEdited() {
    bumpMeshContentRevision();
}

Mesh& TrimeshNodeModel::mesh() {
    if (ownedMesh == nullptr) {
        ownedMesh = TrimeshMeshFactory::createDefaultMesh("Cycle2TrimeshNode");
        bumpMeshContentRevision();
    }

    return *ownedMesh;
}

void TrimeshNodeModel::bumpMeshContentRevision() {
    ++revision;
    ++revisions.meshContent;
    ++revisions.sliceRasterization;
    ++revisions.interceptsRails;
    ++revisions.columns3D;
    ++revisions.compactPreview;
    ++revisions.selectedControl;
    ++revisions.dspPrep;
    revisions.aggregate = revision;
}

void TrimeshNodeModel::bumpMorphRevision() {
    ++revision;
    ++revisions.sliceRasterization;
    ++revisions.interceptsRails;
    ++revisions.columns3D;
    ++revisions.compactPreview;
    ++revisions.dspPrep;
    revisions.aggregate = revision;
}

void TrimeshNodeModel::bumpPrimaryAxisRevision() {
    ++revision;
    ++revisions.sliceRasterization;
    ++revisions.interceptsRails;
    ++revisions.columns3D;
    ++revisions.compactPreview;
    ++revisions.dspPrep;
    revisions.aggregate = revision;
}

void TrimeshNodeModel::bumpSelectedControlRevision() {
    ++revision;
    ++revisions.selectedControl;
    revisions.aggregate = revision;
}

int TrimeshNodeModel::resolvedSelectedVertexIndex() {
    Mesh& activeMesh = mesh();
    auto& verts = activeMesh.getVerts();

    if (isPositiveAndBelow(selectedVertexIndex, (int) verts.size())) {
        return selectedVertexIndex;
    }

    for (auto* cube : activeMesh.getCubes()) {
        if (cube == nullptr) {
            continue;
        }

        if (Vertex* vertex = cube->findClosestVertex(morph)) {
            for (int i = 0; i < (int) verts.size(); ++i) {
                if (verts[(size_t) i] == vertex) {
                    return i;
                }
            }
        }
    }

    if (!verts.empty()) {
        return 0;
    }

    return -1;
}

Vertex* TrimeshNodeModel::selectedVertex() {
    return vertexAtIndex(resolvedSelectedVertexIndex());
}

Vertex* TrimeshNodeModel::vertexAtIndex(int vertexIndex) {
    Mesh& activeMesh = mesh();
    auto& verts = activeMesh.getVerts();

    if (isPositiveAndBelow(vertexIndex, (int) verts.size())) {
        return verts[(size_t) vertexIndex];
    }

    return nullptr;
}

bool TrimeshNodeModel::applySerializedMeshEdits(const TrimeshMeshEditState& nextMeshEditState) {
    const bool changed = nextMeshEditState.applyTo(mesh());
    meshEditState = nextMeshEditState;
    return changed;
}

bool TrimeshNodeModel::applyLegacySelectedVertexOverride(const Node& node) {
    bool changed {};
    float time {};
    float red {};
    float blue {};
    float phase {};
    float amp {};
    float curve {};
    const bool hasTime = parameterFloatValue(node, "vertex.time", time);
    const bool hasRed = parameterFloatValue(node, "vertex.red", red);
    const bool hasBlue = parameterFloatValue(node, "vertex.blue", blue);
    const bool hasPhase = parameterFloatValue(node, "vertex.phase", phase);
    const bool hasAmp = parameterFloatValue(node, "vertex.amp", amp);
    const bool hasCurve = parameterFloatValue(node, "vertex.curve", curve);
    const int overrideIndex = parameterInt(node, "vertexOverrideIndex", selectedVertexIndex);

    if (!hasTime && !hasRed && !hasBlue && !hasPhase && !hasAmp && !hasCurve) {
        return false;
    }

    if (overrideIndex != selectedVertexIndex) {
        return false;
    }

    Vertex* vertex = selectedVertex();

    if (vertex == nullptr) {
        return false;
    }

    auto apply = [&](int index, float value) {
        const float clampedValue = jlimit(0.f, 1.f, value);

        if (vertex->values[index] == clampedValue) {
            return;
        }

        vertex->values[index] = clampedValue;
        changed = true;
    };

    if (hasTime) {
        apply(Vertex::Time, time);
    }

    if (hasRed) {
        apply(Vertex::Red, red);
    }

    if (hasBlue) {
        apply(Vertex::Blue, blue);
    }

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

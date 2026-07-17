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

constexpr TrimeshDerivedProduct renderProducts =
        TrimeshDerivedProduct::SliceRasterization
        | TrimeshDerivedProduct::InterceptsRails
        | TrimeshDerivedProduct::Columns3D
        | TrimeshDerivedProduct::CompactPreview
        | TrimeshDerivedProduct::DspPreparation;

bool includes(
        TrimeshDerivedProduct products,
        TrimeshDerivedProduct product) {
    return ((uint32_t) products & (uint32_t) product) != 0;
}

}

namespace {

float parameterFloat(const Node& node, const String& id, float fallback) {
    for (const auto& parameter : node.parameters) {
        if (parameter.id == id) {
            return parameter.value.getFloatValue();
        }
    }

    return fallback;
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

void normalizeBipolarBlock(SignalPayload& payload) {
    if (payload.block.samples.empty()) {
        return;
    }

    Buffer<float>(payload.block.samples.data(), (int) payload.block.samples.size())
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
    ,   topologySnapshot(std::move(other.topologySnapshot))
    ,   morph           (other.morph)
    ,   primaryViewAxis (other.primaryViewAxis)
    ,   selectedVertexIndex (other.selectedVertexIndex)
    ,   revision        (other.revision)
    ,   revisions       (other.revisions) {}

TrimeshNodeModel& TrimeshNodeModel::operator=(TrimeshNodeModel&& other) noexcept {
    if (this != &other) {
        clearMesh();
        ownedMesh = std::move(other.ownedMesh);
        topologySnapshot = std::move(other.topologySnapshot);
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
    const String nextTopologySnapshot = parameterString(
            node,
            TrimeshMeshState::parameterId(),
            {});

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

    if (applyTopologySnapshot(nextTopologySnapshot)) {
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
    SignalPayload slice;
    blockwiseDsp.setMesh(&mesh());
    blockwiseDsp.setMorphPosition(morph);
    blockwiseDsp.setPrimaryViewAxis(primaryViewAxis);
    blockwiseDsp.setCyclic(cyclic);
    blockwiseDsp.renderCycle((size_t) rows, domain, ChannelLayout::LinkedStereo, slice);
    normalizeBipolarBlock(slice);
    result.slice = std::move(slice.block.samples);

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
                column.signal.block.samples.begin(),
                column.signal.block.samples.end());
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

String TrimeshNodeModel::currentMeshState() {
    if (ownedMesh == nullptr) {
        return {};
    }
    topologySnapshot = TrimeshMeshState::serialize(*ownedMesh);
    return topologySnapshot;
}

Mesh& TrimeshNodeModel::mesh() {
    if (ownedMesh == nullptr) {
        ownedMesh = TrimeshMeshFactory::createDefaultMesh("Cycle2TrimeshNode");
        bumpMeshContentRevision();
    }

    return *ownedMesh;
}

void TrimeshNodeModel::bumpMeshContentRevision() {
    advanceDerivedRevisions(
            TrimeshDerivedProduct::MeshContent
            | renderProducts
            | TrimeshDerivedProduct::SelectedControl);
}

void TrimeshNodeModel::bumpMorphRevision() {
    advanceDerivedRevisions(renderProducts);
}

void TrimeshNodeModel::bumpPrimaryAxisRevision() {
    advanceDerivedRevisions(renderProducts);
}

void TrimeshNodeModel::bumpSelectedControlRevision() {
    advanceDerivedRevisions(TrimeshDerivedProduct::SelectedControl);
}

void TrimeshNodeModel::advanceDerivedRevisions(TrimeshDerivedProduct products) {
    ++revision;
    if (includes(products, TrimeshDerivedProduct::MeshContent)) {
        ++revisions.meshContent;
    }
    if (includes(products, TrimeshDerivedProduct::SliceRasterization)) {
        ++revisions.sliceRasterization;
    }
    if (includes(products, TrimeshDerivedProduct::InterceptsRails)) {
        ++revisions.interceptsRails;
    }
    if (includes(products, TrimeshDerivedProduct::Columns3D)) {
        ++revisions.columns3D;
    }
    if (includes(products, TrimeshDerivedProduct::CompactPreview)) {
        ++revisions.compactPreview;
    }
    if (includes(products, TrimeshDerivedProduct::SelectedControl)) {
        ++revisions.selectedControl;
    }
    if (includes(products, TrimeshDerivedProduct::DspPreparation)) {
        ++revisions.dspPrep;
    }
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

void TrimeshNodeModel::selectVertex(Vertex* vertex) {
    const auto& vertices = mesh().getVerts();
    for (int i = 0; i < (int) vertices.size(); ++i) {
        if (vertices[(size_t) i] == vertex) {
            if (selectedVertexIndex != i) {
                selectedVertexIndex = i;
                bumpSelectedControlRevision();
            }
            return;
        }
    }
}

bool TrimeshNodeModel::setVertexParameter(
        int vertexIndex,
        const String& parameterId,
        float value) {
    Vertex* vertex = vertexAtIndex(vertexIndex);
    const int valueIndex = vertexValueIndex(parameterId);
    if (vertex == nullptr || valueIndex < 0) {
        return false;
    }
    const float clampedValue = jlimit(0.f, 1.f, value);
    if (vertex->values[valueIndex] == clampedValue) {
        return true;
    }
    vertex->values[valueIndex] = clampedValue;
    topologySnapshot.clear();
    bumpMeshContentRevision();
    return true;
}

int TrimeshNodeModel::vertexValueIndex(const String& parameterId) {
    const String field = parameterId.fromLastOccurrenceOf(".", false, false);
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

Vertex* TrimeshNodeModel::vertexAtIndex(int vertexIndex) {
    Mesh& activeMesh = mesh();
    auto& verts = activeMesh.getVerts();

    if (isPositiveAndBelow(vertexIndex, (int) verts.size())) {
        return verts[(size_t) vertexIndex];
    }

    return nullptr;
}

bool TrimeshNodeModel::applyTopologySnapshot(const String& snapshot) {
    if (snapshot.isEmpty() || snapshot == topologySnapshot) {
        return false;
    }
    if (!TrimeshMeshState::apply(snapshot, mesh())) {
        return false;
    }
    topologySnapshot = snapshot;
    return true;
}

void TrimeshNodeModel::clearMesh() {
    if (ownedMesh != nullptr) {
        ownedMesh->destroy();
        ownedMesh = nullptr;
    }
}

}

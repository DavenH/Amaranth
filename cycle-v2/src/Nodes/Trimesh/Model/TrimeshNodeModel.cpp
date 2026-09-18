#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"

#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshVertexEditCore.h"

#include "Graph/NodeParameterMap.h"

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

int primaryAxisFromParameter(const String& axisName) {
    if (axisName == "red") {
        return Vertex::Red;
    }

    if (axisName == "blue") {
        return Vertex::Blue;
    }

    return Vertex::Time;
}

}

TrimeshNodeModel::TrimeshNodeModel() :
        primaryViewAxis(Vertex::Time) {}

TrimeshNodeModel::~TrimeshNodeModel() {
    clearMesh();
}

TrimeshNodeModel::TrimeshNodeModel(TrimeshNodeModel&& other) noexcept :
        ownedMesh            (std::move(other.ownedMesh))
    ,   morph                (other.morph)
    ,   primaryViewAxis      (other.primaryViewAxis)
    ,   selectedVertexIndex  (other.selectedVertexIndex)
    ,   revision             (other.revision)
    ,   appliedModelRevision (other.appliedModelRevision)
    ,   appliedModelState    (std::move(other.appliedModelState))
    ,   guideCurveProvider   (std::move(other.guideCurveProvider))
    ,   revisions            (other.revisions) {}

TrimeshNodeModel& TrimeshNodeModel::operator=(TrimeshNodeModel&& other) noexcept {
    if (this != &other) {
        clearMesh();
        ownedMesh = std::move(other.ownedMesh);
        morph = other.morph;
        primaryViewAxis = other.primaryViewAxis;
        selectedVertexIndex = other.selectedVertexIndex;
        revision = other.revision;
        appliedModelRevision = other.appliedModelRevision;
        appliedModelState = std::move(other.appliedModelState);
        guideCurveProvider = std::move(other.guideCurveProvider);
        revisions = other.revisions;
    }

    return *this;
}

bool TrimeshNodeModel::syncFromNode(
        const Node& node,
        TrimeshSelectionSyncPolicy selectionPolicy) {
    const NodeParameterMap parameters(node);
    const MorphPosition nextMorph {
            parameters.floatValue("yellow", 0.5f),
            parameters.floatValue("red", 0.5f),
            parameters.floatValue("blue", 0.5f)
    };
    const int nextPrimaryAxis = primaryAxisFromParameter(
            parameters.stringValue("primaryAxis", "yellow"));
    const int nextSelectedVertexIndex = (int) node.editorState.getProperty(
            "selectedVertexId", -1);

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

    if (selectionPolicy == TrimeshSelectionSyncPolicy::SynchronizeFromNode
            && nextSelectedVertexIndex != selectedVertexIndex) {
        selectedVertexIndex = nextSelectedVertexIndex;
        bumpSelectedControlRevision();
    }

    const auto typedModel = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node.model);
    if (typedModel != nullptr
            && (typedModel != appliedModelState
                    || typedModel->revision() != appliedModelRevision)) {
        const bool meshReplaced = !mesh().equals(typedModel->mesh());
        if (meshReplaced) {
            mesh().deepCopy(&typedModel->mesh());
            bumpMeshContentRevision();
        }
        appliedModelRevision = typedModel->revision();
        appliedModelState = typedModel;
        return meshReplaced;
    }

    return false;
}

bool TrimeshNodeModel::applyPreparedGuides(
        const Mesh& preparedMesh,
        std::shared_ptr<GuideCurveSnapshotProvider> provider) {
    const bool meshReplaced = !mesh().equals(preparedMesh);
    if (meshReplaced) {
        mesh().deepCopy(&preparedMesh);
    }
    guideCurveProvider = std::move(provider);
    bumpMeshContentRevision();
    return meshReplaced;
}

std::vector<TrimeshVertexParameter> TrimeshNodeModel::getVertexParametersForIndex(int vertexIndex) {
    Vertex* selectedVertex = vertexAtIndex(vertexIndex);

    if (selectedVertex == nullptr) {
        return {};
    }

    const auto parameter = [this, vertexIndex, selectedVertex](
            const String& id,
            const String& label,
            int valueIndex) {
        return TrimeshVertexParameter {
                id,
                label,
                selectedVertex->values[valueIndex],
                0.f,
                1.f,
                vertexGuideGain(vertexIndex, id)
        };
    };

    return {
            parameter("vertex.time", "time", Vertex::Time),
            parameter("vertex.red", "red", Vertex::Red),
            parameter("vertex.blue", "blue", Vertex::Blue),
            parameter("vertex.phase", "phase", Vertex::Phase),
            parameter("vertex.amp", "amp", Vertex::Amp),
            parameter("vertex.curve", "curve", Vertex::Curve)
    };
}

std::vector<TrimeshVertexParameter> TrimeshNodeModel::getSelectedVertexParameters() {
    auto parameters = getVertexParametersForIndex(selectedVertexIndex);
    if (!parameters.empty()) {
        return parameters;
    }

    return {
            { "vertex.time", "time", 0.f, 0.f, 1.f, 0.5f, false },
            { "vertex.red", "red", 0.f, 0.f, 1.f, 0.5f, false },
            { "vertex.blue", "blue", 0.f, 0.f, 1.f, 0.5f, false },
            { "vertex.phase", "phase", 0.f, 0.f, 1.f, 0.5f, false },
            { "vertex.amp", "amp", 0.f, 0.f, 1.f, 0.5f, false },
            { "vertex.curve", "curve", 0.f, 0.f, 1.f, 0.5f, false }
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
    return vertexAtIndex(selectedVertexIndex);
}

bool TrimeshNodeModel::selectVertex(Vertex* vertex) {
    if (vertex == nullptr) {
        if (selectedVertexIndex == -1) {
            return false;
        }
        selectedVertexIndex = -1;
        bumpSelectedControlRevision();
        return true;
    }

    const auto& vertices = mesh().getVerts();
    for (int i = 0; i < (int) vertices.size(); ++i) {
        if (vertices[(size_t) i] == vertex) {
            if (selectedVertexIndex != i) {
                selectedVertexIndex = i;
                bumpSelectedControlRevision();
                return true;
            }
            return false;
        }
    }

    return false;
}

bool TrimeshNodeModel::setVertexParameter(
        int vertexIndex,
        const String& parameterId,
        float value) {
    const auto delta = TrimeshVertexEditCore::prepareVertexValue(
            mesh(), vertexIndex, parameterId, value);
    if (!delta.has_value() || !TrimeshVertexEditCore::apply(mesh(), *delta)) {
        return false;
    }
    if (delta->changed()) {
        bumpMeshContentRevision();
    }
    return true;
}

bool TrimeshNodeModel::setVertexGuideGain(
        int vertexIndex,
        const String& parameterId,
        float value) {
    const auto delta = TrimeshVertexEditCore::prepareGuideGain(
            mesh(), vertexIndex, parameterId, value);
    if (!delta.has_value() || !TrimeshVertexEditCore::apply(mesh(), *delta)) {
        return false;
    }
    if (delta->changed()) {
        bumpMeshContentRevision();
    }
    return true;
}

float TrimeshNodeModel::vertexGuideGain(
        int vertexIndex,
        const String& parameterId) {
    return TrimeshVertexEditCore::guideGain(mesh(), vertexIndex, parameterId);
}

Vertex* TrimeshNodeModel::vertexAtIndex(int vertexIndex) {
    Mesh& activeMesh = mesh();
    auto& verts = activeMesh.getVerts();

    if (isPositiveAndBelow(vertexIndex, (int) verts.size())) {
        return verts[(size_t) vertexIndex];
    }

    return nullptr;
}

void TrimeshNodeModel::clearMesh() {
    if (ownedMesh != nullptr) {
        ownedMesh->destroy();
        ownedMesh = nullptr;
    }
}

}

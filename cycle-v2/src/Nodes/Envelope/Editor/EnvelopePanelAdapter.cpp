#include "Nodes/Envelope/Editor/EnvelopePanelAdapter.h"

#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/NodeParameterMap.h"

#include "Nodes/Envelope/EnvelopeMeshState.h"

#include <Curve/Mesh/VertCube.h>
#include <Curve/Mesh/MorphDimensions.h>
#include <Obj/MorphPosition.h>

#include <algorithm>

namespace CycleV2 {

EnvelopePanelAdapter::EnvelopePanelAdapter() = default;

EnvelopePanelAdapter::~EnvelopePanelAdapter() = default;

bool EnvelopePanelAdapter::needsNodeSync(const Node& node) const {
    if (node.kind != NodeKind::Envelope) {
        return false;
    }
    return node.model != nullptr
            && node.model->schemaId() == "envelope"
            && (syncedNodeId != node.id || syncedModel != node.model);
}

bool EnvelopePanelAdapter::syncFromNode(const Node& node) {
    if (node.kind != NodeKind::Envelope) {
        return false;
    }
    if (!needsNodeSync(node)) {
        return false;
    }
    if (!model.syncFromNode(node)) {
        return false;
    }
    const bool differentNode = syncedNodeId != node.id;
    syncedNodeId = node.id;
    syncedModel = node.model;
    const NodeParameterMap parameters(node);
    dimensions = parameters.intValue("dimensionCount", 0);
    authoredMorph = { parameters.floatValue("yellow", 0.f),
            parameters.floatValue("red", 0.5f), parameters.floatValue("blue", 0.5f) };
    refreshEffectiveMorph();
    if (differentNode) {
        morphRedLinked = true;
        morphBlueLinked = true;
    }
    model.selectCube((EnvelopeCubeId) (int64) node.editorState.getProperty("selectedCubeId", 0));
    model.setPublicationRevision(node.model->revision());
    return true;
}

void EnvelopePanelAdapter::initialiseDefaultMesh() {
    if (mesh().getNumVerts() == 0) {
        EnvelopeMeshState::initialiseDefault(mesh());
    }
}

String EnvelopePanelAdapter::serializedMeshState() {
    InteractionComplexityDiagnostics::recordModelSerialization();
    return EnvelopeMeshState::serialize(mesh());
}

NodeModelStatePtr EnvelopePanelAdapter::modelPublication(
        VertCube* selectedCube,
        uint64_t publicationRevision) {
    if (!model.synchronizeFromMesh(selectedCube)) {
        return {};
    }
    model.setPublicationRevision(publicationRevision);
    auto editor = std::make_unique<DynamicObject>();
    if (model.selectedCubeId().has_value()) {
        editor->setProperty("selectedCubeId", (int64) *model.selectedCubeId());
    }
    return CurveNodeModelState::copyOf(
            model, authoredMorph.red, authoredMorph.blue, publicationRevision, var(editor.release()));
}

std::vector<CurvePreviewVertex> EnvelopePanelAdapter::previewVertices() {
    std::vector<CurvePreviewVertex> result;
    result.reserve((size_t) mesh().getNumCubes());
    MorphPosition position;
    position.time.setValueDirect(morphYellow);
    position.red.setValueDirect(morphRed);
    position.blue.setValueDirect(morphBlue);
    position.timeDepth = 0.001f;
    position.redDepth = 1.f;
    position.blueDepth = 1.f;

    for (VertCube* cube : mesh().getCubes()) {
        if (cube == nullptr) {
            continue;
        }
        VertCube::ReductionData reduction;
        cube->getFinalIntercept(reduction, position);
        if (reduction.pointOverlaps) {
            result.push_back({
                    reduction.v.values[Vertex::Phase],
                    reduction.v.values[Vertex::Amp],
                    reduction.v.values[Vertex::Curve]
            });
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.x < rhs.x;
    });
    return result;
}

bool EnvelopePanelAdapter::registerMeshEdit() {
    return true;
}

void EnvelopePanelAdapter::setMorph(const MorphPosition& position) {
    authoredMorph = position;
    refreshEffectiveMorph();
}

bool EnvelopePanelAdapter::setPreviewMorphPosition(std::optional<MorphPosition> position) {
    const MorphPosition previous = effectiveMorphPosition();
    previewMorph = std::move(position);
    refreshEffectiveMorph();
    return previous.time != morphYellow || previous.red != morphRed || previous.blue != morphBlue;
}

void EnvelopePanelAdapter::refreshEffectiveMorph() {
    const MorphPosition position = previewMorph.has_value()
            ? MorphDimensions(dimensions).constrain(*previewMorph, authoredMorph) : authoredMorph;
    morphYellow = position.time;
    morphRed = position.red;
    morphBlue = position.blue;
}

void EnvelopePanelAdapter::setLogarithmic(bool logarithmicToUse) {
    model.logarithmic = logarithmicToUse;
}

void EnvelopePanelAdapter::setAxisLinks(bool redLinkedToUse, bool blueLinkedToUse) {
    morphRedLinked = redLinkedToUse;
    morphBlueLinked = blueLinkedToUse;
}

}

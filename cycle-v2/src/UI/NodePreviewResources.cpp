#include "UI/NodePreviewResources.h"

#include "Graph/GraphEditTypes.h"
#include "UI/NodeEditorHost.h"

#include "Nodes/Effects/EffectSignalProcessors.h"
#include "Runtime/FingerprintBuilder.h"

namespace CycleV2 {

NodePreviewResources::NodePreviewResources(NodeEditorCommandService& commands) :
        editorCommands(commands) {
}

void NodePreviewResources::setDurableGraph(const NodeGraph* graphToUse) {
    durableGraph = graphToUse;
    if (durableGraph != nullptr) {
        previewPitchContexts.rebuild(*durableGraph);
    }
}

void NodePreviewResources::refreshGraph(
        const NodeGraph& graphToUse,
        const GraphChangeSet& changes) {
    previewPitchContexts.applyParameterChanges(
            graphToUse, changes.nodeIds, changes.topologyChanged);
    if (!changes.guidesChanged) {
        return;
    }
    for (const String& nodeId : changes.nodeIds) {
        const Node* node = graphToUse.findNode(nodeId);
        if (node == nullptr) {
            continue;
        }
        if (trimeshWidgetIndices.contains(nodeId)) {
            auto& widget = *trimeshWidgets[(size_t) trimeshWidgetIndices[nodeId]].second;
            widget.syncFromNode(*node);
            widget.syncGuideContext(graphToUse, *node);
            initializedTrimeshGuideContexts.addIfNotAlreadyThere(nodeId);
        }
        if (node->kind == NodeKind::Envelope
                && curveEditorWidgetIndices.contains(nodeId)) {
            auto& widget = *curveEditorWidgets[
                    (size_t) curveEditorWidgetIndices[nodeId]].second;
            widget.syncFromNode(*node);
            widget.syncGuideContext(graphToUse, *node);
            initializedCurveGuideContexts.addIfNotAlreadyThere(nodeId);
        }
    }
}

TrimeshWidget& NodePreviewResources::trimeshWidget(const String& nodeId) {
    if (trimeshWidgetIndices.contains(nodeId)) {
        return *trimeshWidgets[(size_t) trimeshWidgetIndices[nodeId]].second;
    }

    trimeshWidgets.emplace_back(nodeId, std::make_unique<TrimeshWidget>());
    trimeshWidgetIndices.set(nodeId, (int) trimeshWidgets.size() - 1);
    TrimeshWidget& widget = *trimeshWidgets.back().second;
    widget.setMeshEditedCallback([this, nodeId](TrimeshMeshEditEvent event) {
        if (event.selectionOnly) {
            editorCommands.selectTrimeshVertexIndex(
                    nodeId,
                    trimeshWidget(nodeId).selectedVertexIndexForPanel());
        } else {
            editorCommands.persistTrimeshMeshEdits(nodeId, event.gestureComplete);
        }
    });
    return widget;
}

TrimeshWidget& NodePreviewResources::trimeshWidget(const Node& node) {
    TrimeshWidget& widget = trimeshWidget(node.id);
    const PreviewPitchContext preview = previewPitchContexts
            .contextForNodeAtPreviewNote(node.id, selectedPreviewMidiNote);
    widget.setPreviewMidiNote(preview.midiNote);
    widget.setPreviewKeyScaleAxis(preview.keyScaleAxis);
    widget.syncFromNode(node);
    if (durableGraph != nullptr
            && !initializedTrimeshGuideContexts.contains(node.id)) {
        widget.syncGuideContext(*durableGraph, node);
        initializedTrimeshGuideContexts.add(node.id);
    }
    return widget;
}

CurveEditorWidget& NodePreviewResources::curveEditorWidget(const Node& node) {
    const bool created = !curveEditorWidgetIndices.contains(node.id);
    if (created) {
        curveEditorWidgets.emplace_back(node.id, std::make_unique<CurveEditorWidget>(node.kind));
        curveEditorWidgetIndices.set(node.id, (int) curveEditorWidgets.size() - 1);
    }
    CurveEditorWidget* widget = curveEditorWidgets[
            (size_t) curveEditorWidgetIndices[node.id]].second.get();
    if (created && node.kind == NodeKind::ImpulseResponse) {
        widget->setImpulseResponseAudioResource(
                IrSignalProcessor::directResource(durableGraph, node.id));
    }
    return *widget;
}

void NodePreviewResources::syncCurveEditorWidget(const Node& node) {
    CurveEditorWidget& widget = curveEditorWidget(node);
    if (node.kind == NodeKind::ImpulseResponse) {
        widget.setImpulseResponseAudioResource(
                IrSignalProcessor::directResource(durableGraph, node.id));
    }
    widget.syncFromNode(node);
    if (durableGraph != nullptr
            && node.kind == NodeKind::Envelope
            && !initializedCurveGuideContexts.contains(node.id)) {
        widget.syncGuideContext(*durableGraph, node);
        initializedCurveGuideContexts.add(node.id);
    }
}

CachedNodePreviewSprite& NodePreviewResources::cachedSprite(const String& nodeId) {
    for (auto& entry : cachedSprites) {
        if (entry.first == nodeId) {
            return entry.second;
        }
    }

    cachedSprites.emplace_back(nodeId, CachedNodePreviewSprite {});
    return cachedSprites.back().second;
}

uint64_t NodePreviewResources::nodePresentationFingerprint(const String& nodeId) const {
    FingerprintBuilder fingerprint;
    if (curveEditorWidgetIndices.contains(nodeId)) {
        const auto& widget = curveEditorWidgets[
                (size_t) curveEditorWidgetIndices[nodeId]].second;
        return fingerprint
                .add(widget->contentRevision())
                .add(widget->previewRevision())
                .add(widget->previewSnapshotRevision())
                .value();
    }
    if (trimeshWidgetIndices.contains(nodeId)) {
        const auto& widget = trimeshWidgets[
                (size_t) trimeshWidgetIndices[nodeId]].second;
        return fingerprint.add(widget->guideContextKey()).value();
    }
    return fingerprint.value();
}

const TrimeshWidget* NodePreviewResources::findTrimeshWidget(const String& nodeId) const {
    if (trimeshWidgetIndices.contains(nodeId)) {
        return trimeshWidgets[(size_t) trimeshWidgetIndices[nodeId]].second.get();
    }
    return nullptr;
}

TrimeshWidget* NodePreviewResources::findTrimeshWidget(const String& nodeId) {
    return const_cast<TrimeshWidget*>(
            std::as_const(*this).findTrimeshWidget(nodeId));
}

void NodePreviewResources::clearCachedSprites() {
    cachedSprites.clear();
}

void NodePreviewResources::resetDocumentPreviews() {
    clearCachedSprites();
    initializedTrimeshGuideContexts.clear();
    initializedCurveGuideContexts.clear();
    for (auto& entry : curveEditorWidgets) {
        entry.second->resetDocumentPresentation();
    }
}

void NodePreviewResources::releaseOpenGLResources() {
    for (auto& entry : trimeshWidgets) {
        entry.second->releaseSharedGlResources();
    }

    for (auto& entry : curveEditorWidgets) {
        entry.second->releaseSharedGlResources();
    }
}

void NodePreviewResources::hideExpandedHostsExcept(const String& nodeId) {
    for (auto& entry : trimeshWidgets) {
        if (entry.first == nodeId) {
            continue;
        }

        Component* panel3D = entry.second->getExpandedPanel3DComponentIfCreated();
        Component* panel2D = entry.second->getExpandedPanel2DComponentIfCreated();

        if (panel3D != nullptr && panel3D->isVisible()) {
            panel3D->setVisible(false);
        }

        if (panel2D != nullptr && panel2D->isVisible()) {
            panel2D->setVisible(false);
        }
    }

    for (auto& entry : curveEditorWidgets) {
        if (entry.first == nodeId) {
            continue;
        }

        Component* panel = entry.second->getExpandedPanelComponentIfCreated();

        if (panel != nullptr && panel->isVisible()) {
            panel->setVisible(false);
        }
    }
}

void NodePreviewResources::detachTrimeshHosts(Component& parent) {
    for (auto& entry : trimeshWidgets) {
        Component* panel3D = entry.second->getExpandedPanel3DComponentIfCreated();
        Component* panel2D = entry.second->getExpandedPanel2DComponentIfCreated();

        if (panel3D != nullptr && panel3D->getParentComponent() == &parent) {
            parent.removeChildComponent(panel3D);
        }

        if (panel2D != nullptr && panel2D->getParentComponent() == &parent) {
            parent.removeChildComponent(panel2D);
        }
    }
}

}

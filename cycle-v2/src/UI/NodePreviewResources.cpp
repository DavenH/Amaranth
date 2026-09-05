#include "UI/NodePreviewResources.h"

#include "UI/NodeEditorHost.h"

#include "Nodes/Effects/EffectSignalProcessors.h"
#include "Runtime/FingerprintBuilder.h"
#include "Runtime/PreviewPitchResolver.h"

namespace CycleV2 {

NodePreviewResources::NodePreviewResources(NodeEditorCommandService& commands) :
        editorCommands(commands) {
}

TrimeshWidget& NodePreviewResources::trimeshWidget(const String& nodeId) {
    for (auto& entry : trimeshWidgets) {
        if (entry.first == nodeId) {
            return *entry.second;
        }
    }

    trimeshWidgets.emplace_back(nodeId, std::make_unique<TrimeshWidget>());
    TrimeshWidget& widget = *trimeshWidgets.back().second;
    widget.setMeshEditedCallback([this, nodeId](TrimeshMeshEditEvent event) {
        editorCommands.persistTrimeshMeshEdits(nodeId, event.gestureComplete);
    });
    return widget;
}

TrimeshWidget& NodePreviewResources::trimeshWidget(const Node& node) {
    TrimeshWidget& widget = trimeshWidget(node.id);
    const PreviewPitchContext preview = graph != nullptr
            ? PreviewPitchResolver::contextForNode(*graph, node.id)
            : PreviewPitchContext {};
    widget.setPreviewMidiNote(preview.midiNote);
    widget.setPreviewKeyScaleAxis(preview.keyScaleAxis);
    widget.syncFromNode(node);
    if (graph != nullptr) {
        widget.syncGuideContext(*graph, node);
    }
    return widget;
}

CurveEditorWidget& NodePreviewResources::curveEditorWidget(const Node& node) {
    CurveEditorWidget* widget {};
    bool created {};
    for (auto& entry : curveEditorWidgets) {
        if (entry.first == node.id) {
            widget = entry.second.get();
            break;
        }
    }

    if (widget == nullptr) {
        curveEditorWidgets.emplace_back(node.id, std::make_unique<CurveEditorWidget>(node.kind));
        widget = curveEditorWidgets.back().second.get();
        created = true;
    }
    if (created && node.kind == NodeKind::ImpulseResponse) {
        widget->setImpulseResponseAudioResource(
                IrSignalProcessor::directResource(graph, node.id));
    }
    return *widget;
}

void NodePreviewResources::syncCurveEditorWidget(const Node& node) {
    CurveEditorWidget& widget = curveEditorWidget(node);
    if (node.kind == NodeKind::ImpulseResponse) {
        widget.setImpulseResponseAudioResource(
                IrSignalProcessor::directResource(graph, node.id));
    }
    widget.syncFromNode(node);
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
    for (const auto& entry : curveEditorWidgets) {
        if (entry.first == nodeId) {
            return fingerprint
                    .add(entry.second->contentRevision())
                    .add(entry.second->previewRevision())
                    .value();
        }
    }
    for (const auto& entry : trimeshWidgets) {
        if (entry.first == nodeId) {
            return fingerprint.add(entry.second->guideContextKey()).value();
        }
    }
    return fingerprint.value();
}

const TrimeshWidget* NodePreviewResources::findTrimeshWidget(const String& nodeId) const {
    for (const auto& entry : trimeshWidgets) {
        if (entry.first == nodeId) {
            return entry.second.get();
        }
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

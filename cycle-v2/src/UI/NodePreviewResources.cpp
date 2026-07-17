#include "NodePreviewResources.h"

#include "NodeEditorHost.h"

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
    widget.setMeshEditedCallback([this, nodeId] {
        editorCommands.persistTrimeshMeshEdits(nodeId);
    });
    return widget;
}

Effect2DWidget& NodePreviewResources::effect2DWidget(const Node& node) {
    for (auto& entry : effect2DWidgets) {
        if (entry.first == node.id) {
            return *entry.second;
        }
    }

    effect2DWidgets.emplace_back(node.id, std::make_unique<Effect2DWidget>(node.kind));
    return *effect2DWidgets.back().second;
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

const TrimeshWidget* NodePreviewResources::findTrimeshWidget(const String& nodeId) const {
    for (const auto& entry : trimeshWidgets) {
        if (entry.first == nodeId) {
            return entry.second.get();
        }
    }

    return nullptr;
}

void NodePreviewResources::clearCachedSprites() {
    cachedSprites.clear();
}

void NodePreviewResources::releaseOpenGLResources() {
    for (auto& entry : trimeshWidgets) {
        entry.second->releaseSharedGlResources();
    }

    for (auto& entry : effect2DWidgets) {
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

    for (auto& entry : effect2DWidgets) {
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

#include "NodeCanvasEditorCoordinator.h"

#include "NodeViewModule.h"
#include "../Graph/GraphCommandDispatcher.h"
#include "../Graph/GraphDocument.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentMenu.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"

namespace CycleV2 {

namespace {

constexpr float kMinimumEditorMargin = 18.f;

bool hasHostedEditor(const Node& node) {
    return NodeViewModuleRegistry::instance().moduleFor(node.kind).capabilities().hostedEditor;
}

bool hasEffect2DModel(const Node& node) {
    return node.kind == NodeKind::Envelope
            || node.kind == NodeKind::GuideCurve
            || node.kind == NodeKind::ImpulseResponse
            || node.kind == NodeKind::Waveshaper;
}

Rectangle<float> closeButton(const Node& node, Rectangle<float> panel) {
    const bool compact = node.kind == NodeKind::VoiceContext
            || node.kind == NodeKind::Fft
            || node.kind == NodeKind::Ifft;
    return compact
            ? Rectangle<float>(18.f, 18.f).withCentre({ panel.getRight() - 18.f, panel.getY() + 15.f })
            : Rectangle<float>(22.f, 22.f).withCentre({ panel.getRight() - 22.f, panel.getY() + 17.f });
}

}

NodeCanvasEditorCoordinator::NodeCanvasEditorCoordinator(
        Component& ownerToUse,
        GraphDocument& documentToUse,
        NodeEditorCommandService& commandsToUse,
        NodeEditorPresentation& presentationToUse,
        NodeEditorResources& editorResources,
        NodeCanvasEditorState stateToUse) :
        owner(ownerToUse)
    ,   document(documentToUse)
    ,   presentation(presentationToUse)
    ,   state(stateToUse)
    ,   resources(commandsToUse)
    ,   renderer(resources)
    ,   editorHost(owner, commandsToUse, presentation, editorResources) {
}

NodeCanvasEditorCoordinator::~NodeCanvasEditorCoordinator() {
    detach();
}

Rectangle<float> NodeCanvasEditorCoordinator::boundsFor(
        const Node* node,
        Rectangle<float> canvasBounds) {
    if (node == nullptr) {
        const Rectangle<float> available = canvasBounds.reduced(kMinimumEditorMargin);
        return Rectangle<float>(
                jmin(available.getWidth(), jmax(420.f, canvasBounds.getWidth() * 0.90f)),
                jmin(available.getHeight(), jmax(300.f, canvasBounds.getHeight() * 0.90f)))
                .withCentre(available.getCentre());
    }

    return NodeViewModuleRegistry::instance().moduleFor(node->kind).expandedEditorBounds(
            canvasBounds,
            kMinimumEditorMargin);
}

bool NodeCanvasEditorCoordinator::blocksCanvas(const Node* node) {
    return node != nullptr
            && NodeViewModuleRegistry::instance().moduleFor(node->kind)
                    .capabilities().expandedEditorBlocksCanvas;
}

ExpandedEditorClick NodeCanvasEditorCoordinator::routeClick(
        const Node* node,
        Rectangle<float> canvasBounds,
        Point<float> position) {
    if (node == nullptr) {
        return {};
    }

    const Rectangle<float> panel = boundsFor(node, canvasBounds);
    if (hasHostedEditor(*node)) {
        return { ExpandedEditorClickKind::Captured };
    }
    if (closeButton(*node, panel).contains(position)) {
        return { ExpandedEditorClickKind::Close };
    }
    if (!panel.contains(position)) {
        return {};
    }

    if (node->kind == NodeKind::VoiceContext) {
        auto edit = VoiceContextCompactEditor::editAt(*node, panel, position);
        return edit.has_value()
                ? ExpandedEditorClick { ExpandedEditorClickKind::VoiceContextEdit, std::move(edit), {} }
                : ExpandedEditorClick { ExpandedEditorClickKind::Captured };
    }
    if (node->kind == NodeKind::Fft || node->kind == NodeKind::Ifft) {
        auto mode = TransformCompactEditor::modeAt(*node, panel, position);
        return mode.has_value()
                ? ExpandedEditorClick { ExpandedEditorClickKind::TransformMode, {}, mode }
                : ExpandedEditorClick { ExpandedEditorClickKind::Captured };
    }

    return { ExpandedEditorClickKind::Captured };
}

void NodeCanvasEditorCoordinator::updateHost(
        const Node* node,
        Rectangle<float> availableBounds) {
    resources.hideExpandedHostsExcept(node != nullptr ? node->id : String());
    const Rectangle<int> bounds = node != nullptr
            ? boundsFor(node, availableBounds).toNearestInt()
            : Rectangle<int>();
    editorHost.bind(node, bounds, document.revision());
}

void NodeCanvasEditorCoordinator::renderOpenGL(float scaleFactor) {
    editorHost.renderOpenGL(scaleFactor);
}

void NodeCanvasEditorCoordinator::syncEffectNodes(const NodeGraph& graph) {
    for (const auto& node : graph.getNodes()) {
        if (hasEffect2DModel(node)) {
            resources.effect2DWidget(node).syncFromNode(node);
        }
    }
}

void NodeCanvasEditorCoordinator::releaseOpenGLResources() {
    resources.releaseOpenGLResources();
}

void NodeCanvasEditorCoordinator::clearPreviewCache() {
    resources.clearCachedSprites();
}

void NodeCanvasEditorCoordinator::close() {
    state.expandedNodeId = {};
    editorHost.close();
    presentation.repaintNodeEditor(true);
}

void NodeCanvasEditorCoordinator::detach() {
    resources.detachTrimeshHosts(owner);
    editorHost.detach();
}

std::array<String, 6> NodeCanvasEditorCoordinator::trimeshGuideLabelsFor(const Node& node) {
    std::array<String, 6> labels;
    if (node.kind != NodeKind::TrilinearMesh) {
        return labels;
    }

    TrimeshWidget& widget = resources.trimeshWidget(node.id);
    const int vertexIndex = widget.resolvedSelectedVertexIndexForNode(node);
    const auto& fields = TrimeshGuideAttachmentTarget::fields();

    for (int index = 0; index < (int) fields.size(); ++index) {
        const auto items = TrimeshGuideAttachmentMenu::itemsFor(
                document.graph(),
                node.id,
                vertexIndex,
                fields[(size_t) index]);

        for (const auto& item : items) {
            if (item.attached) {
                labels[(size_t) index] = item.label;
                break;
            }
        }
    }

    return labels;
}

}

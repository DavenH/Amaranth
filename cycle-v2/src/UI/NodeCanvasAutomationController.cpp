#include "NodeCanvasAutomationController.h"

#include "NodeViewModule.h"

namespace CycleV2 {

namespace {

var rectangleToVar(Rectangle<float> bounds) {
    auto* object = new DynamicObject();
    object->setProperty("x", bounds.getX());
    object->setProperty("y", bounds.getY());
    object->setProperty("width", bounds.getWidth());
    object->setProperty("height", bounds.getHeight());
    return object;
}

var componentDiagnosticsToVar(const String& id, const Component* component) {
    auto* object = new DynamicObject();
    object->setProperty("id", id);
    object->setProperty("created", component != nullptr);

    if (component == nullptr) {
        return object;
    }

    object->setProperty("visible", component->isVisible());
    object->setProperty("showing", component->isShowing());
    object->setProperty("enabled", component->isEnabled());
    object->setProperty("bounds", rectangleToVar(component->getBounds().toFloat()));
    object->setProperty("screenBounds", rectangleToVar(component->getScreenBounds().toFloat()));
    object->setProperty("hasParent", component->getParentComponent() != nullptr);
    object->setProperty("width", component->getWidth());
    object->setProperty("height", component->getHeight());
    object->setProperty("nonEmpty", !component->getLocalBounds().isEmpty());
    return object;
}

const Node* findNode(const NodeGraph& graph, const String& nodeId) {
    for (const auto& node : graph.getNodes()) {
        if (node.id == nodeId) {
            return &node;
        }
    }

    return nullptr;
}

}

NodeCanvasAutomationController::NodeCanvasAutomationController(
        NodeCanvasAutomationControllerContext contextToUse) :
        context(contextToUse)
    ,   inspector({
            context.canvas,
            context.document,
            context.presentation,
            context.viewport,
            context.editorHost
        }) {
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::openEditor(const String& nodeId) {
    return context.authoring.openEditor(nodeId);
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::addNode(
        const String& kindId,
        Point<float> position) {
    const auto kind = parseNodeKind(kindId);
    if (!kind.has_value()) {
        return {};
    }

    return context.authoring.addNode(*kind, position);
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::moveNode(
        const String& nodeId,
        Point<float> position) {
    return context.authoring.moveNode(nodeId, position);
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::connectPorts(
        const String& sourceNodeId,
        const String& sourcePortId,
        const String& destNodeId,
        const String& destPortId) {
    return context.authoring.connectPorts(
            { sourceNodeId, sourcePortId, false },
            { destNodeId, destPortId, true });
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::deleteNode(const String& nodeId) {
    return context.authoring.deleteNode(nodeId);
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::deleteEdge(int edgeIndex) {
    return context.authoring.deleteEdge(edgeIndex);
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::setNodeParameter(
        const String& nodeId,
        const String& parameterId,
        const String& label,
        const String& value) {
    return context.authoring.setNodeParameter(
            nodeId,
            parameterId,
            label.isEmpty() ? parameterId : label,
            value);
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::setMorph(
        const String& nodeId,
        const String& axis,
        float value) {
    return context.authoring.setTrimeshMorph(nodeId, axis, value);
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::setPrimaryAxis(
        const String& nodeId,
        const String& axis) {
    return context.authoring.setTrimeshPrimaryAxis(nodeId, axis);
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::toggleLink(
        const String& nodeId,
        const String& axis) {
    return context.authoring.toggleTrimeshLink(nodeId, axis);
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::selectVertex(
        const String& nodeId,
        int vertexIndex) {
    return context.authoring.selectTrimeshVertex(nodeId, vertexIndex);
}

NodeCanvasAuthoringResult NodeCanvasAutomationController::setVertexParameter(
        const String& nodeId,
        const String& parameterId,
        float value) {
    return context.authoring.setTrimeshVertexParameter(nodeId, parameterId, value);
}

bool NodeCanvasAutomationController::getNodeParameter(
        const String& nodeId,
        const String& parameterId,
        String& value) const {
    return context.authoring.getNodeParameter(nodeId, parameterId, value);
}

var NodeCanvasAutomationController::exportState(
        const NodeCanvasAutomationPresentation& state) const {
    return inspector.exportState(state);
}

String NodeCanvasAutomationController::exportGraphXml() const {
    return inspector.exportGraphXml();
}

var NodeCanvasAutomationController::inspectNodeControls(
        const String& nodeId,
        const NodeCanvasAutomationPresentation& state) const {
    return inspector.inspectNodeControls(nodeId, state);
}

var NodeCanvasAutomationController::inspectPointerTargets(
        const NodeCanvasAutomationPresentation& state) const {
    return inspector.inspectPointerTargets(state);
}

var NodeCanvasAutomationController::inspectOpenGLDiagnostics(
        const NodeCanvasOpenGLDiagnosticsState& state) const {
    auto* root = new DynamicObject();
    root->setProperty("schema", "cycle-v2-opengl-diagnostics.v1");
    root->setProperty("canvasOpenGlAttached", state.canvasAttached);
    root->setProperty("canvasBounds", rectangleToVar(context.canvas.getLocalBounds().toFloat()));
    root->setProperty("canvasScreenBounds", rectangleToVar(context.canvas.getScreenBounds().toFloat()));
    root->setProperty("expandedNodeId", state.expandedNodeId);
    root->setProperty("trimeshExpandedEditorCreated", context.editorHost.hasEditor());

    if (Component* component = context.editorHost.component()) {
        root->setProperty("trimeshExpandedEditor", componentDiagnosticsToVar(
                "hostedExpandedEditor",
                component));
    }

    const Node* expandedNode = findNode(context.document.graph(), state.expandedNodeId);
    if (expandedNode != nullptr) {
        root->setProperty("expandedNodeKind", labelForNodeKind(expandedNode->kind));
        root->setProperty("expandedEditorBounds", rectangleToVar(
                NodeViewModuleRegistry::instance().moduleFor(expandedNode->kind).expandedEditorBounds(
                        context.canvas.getLocalBounds().toFloat(),
                        18.f)));
    }

    Array<var> panels;
    if (expandedNode != nullptr && expandedNode->kind == NodeKind::TrilinearMesh) {
        const TrimeshWidget* widget = context.previewResources.findTrimeshWidget(expandedNode->id);
        panels.add(componentDiagnosticsToVar(
                "trimeshPanel3D",
                widget == nullptr ? nullptr : widget->getExpandedPanel3DComponentIfCreated()));
        panels.add(componentDiagnosticsToVar(
                "trimeshPanel2D",
                widget == nullptr ? nullptr : widget->getExpandedPanel2DComponentIfCreated()));
    }

    root->setProperty("panels", panels);
    root->setProperty("panelCount", panels.size());
    return root;
}

var NodeCanvasAutomationController::captureAudio(size_t frameCount) const {
    return inspector.captureAudio(frameCount);
}

std::optional<NodeKind> NodeCanvasAutomationController::parseNodeKind(const String& id) {
    static const std::pair<const char*, NodeKind> identifiers[] {
        { "genericProcessor", NodeKind::GenericProcessor },
        { "processor", NodeKind::GenericProcessor },
        { "voiceContext", NodeKind::VoiceContext },
        { "voice", NodeKind::VoiceContext },
        { "waveSource", NodeKind::WaveSource },
        { "wave", NodeKind::WaveSource },
        { "imageSource", NodeKind::ImageSource },
        { "image", NodeKind::ImageSource },
        { "trilinearMesh", NodeKind::TrilinearMesh },
        { "mesh", NodeKind::TrilinearMesh },
        { "fft", NodeKind::Fft },
        { "ifft", NodeKind::Ifft },
        { "envelope", NodeKind::Envelope },
        { "env", NodeKind::Envelope },
        { "add", NodeKind::Add },
        { "multiply", NodeKind::Multiply },
        { "guideCurve", NodeKind::GuideCurve },
        { "guide", NodeKind::GuideCurve },
        { "impulseResponse", NodeKind::ImpulseResponse },
        { "ir", NodeKind::ImpulseResponse },
        { "waveshaper", NodeKind::Waveshaper },
        { "reverb", NodeKind::Reverb },
        { "delay", NodeKind::Delay },
        { "spy", NodeKind::Spy },
        { "stereoSplit", NodeKind::StereoSplit },
        { "split", NodeKind::StereoSplit },
        { "stereoJoin", NodeKind::StereoJoin },
        { "join", NodeKind::StereoJoin },
        { "output", NodeKind::Output },
        { "out", NodeKind::Output }
    };

    for (const auto& identifier : identifiers) {
        if (id == identifier.first) {
            return identifier.second;
        }
    }

    return std::nullopt;
}

}

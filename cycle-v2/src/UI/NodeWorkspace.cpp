#include "NodeWorkspace.h"

namespace CycleV2 {

NodeWorkspace::NodeWorkspace() {
    setOpaque(true);
    addAndMakeVisible(canvas);
}

bool NodeWorkspace::saveGraphToFile(const File& file) {
    return canvas.saveGraphToFile(file);
}

bool NodeWorkspace::loadGraphFromFile(const File& file) {
    return canvas.loadGraphFromFile(file);
}

var NodeWorkspace::exportAutomationState() const {
    return canvas.exportAutomationState();
}

String NodeWorkspace::exportGraphXml() const {
    return canvas.exportGraphXml();
}

bool NodeWorkspace::openNodeEditorForAutomation(const String& nodeId) {
    return canvas.openNodeEditorForAutomation(nodeId);
}

bool NodeWorkspace::addNodeForAutomation(const String& kind, Point<float> position, String& nodeId) {
    return canvas.addNodeForAutomation(kind, position, nodeId);
}

bool NodeWorkspace::moveNodeForAutomation(const String& nodeId, Point<float> position) {
    return canvas.moveNodeForAutomation(nodeId, position);
}

bool NodeWorkspace::connectPortsForAutomation(
        const String& sourceNodeId,
        const String& sourcePortId,
        const String& destNodeId,
        const String& destPortId) {
    return canvas.connectPortsForAutomation(sourceNodeId, sourcePortId, destNodeId, destPortId);
}

bool NodeWorkspace::deleteNodeForAutomation(const String& nodeId) {
    return canvas.deleteNodeForAutomation(nodeId);
}

bool NodeWorkspace::deleteEdgeForAutomation(int edgeIndex) {
    return canvas.deleteEdgeForAutomation(edgeIndex);
}

bool NodeWorkspace::setNodeParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        const String& label,
        const String& value) {
    return canvas.setNodeParameterForAutomation(nodeId, parameterId, label, value);
}

bool NodeWorkspace::setMorphSliderForAutomation(const String& nodeId, const String& axis, float value) {
    return canvas.setMorphSliderForAutomation(nodeId, axis, value);
}

bool NodeWorkspace::setPrimaryAxisForAutomation(const String& nodeId, const String& axis) {
    return canvas.setPrimaryAxisForAutomation(nodeId, axis);
}

bool NodeWorkspace::toggleLinkForAutomation(const String& nodeId, const String& axis) {
    return canvas.toggleLinkForAutomation(nodeId, axis);
}

bool NodeWorkspace::selectVertexForAutomation(const String& nodeId, int vertexIndex) {
    return canvas.selectVertexForAutomation(nodeId, vertexIndex);
}

bool NodeWorkspace::setVertexParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        float value) {
    return canvas.setVertexParameterForAutomation(nodeId, parameterId, value);
}

bool NodeWorkspace::getNodeParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        String& value) const {
    return canvas.getNodeParameterForAutomation(nodeId, parameterId, value);
}

var NodeWorkspace::inspectNodeControlsForAutomation(const String& nodeId) const {
    return canvas.inspectNodeControlsForAutomation(nodeId);
}

var NodeWorkspace::inspectPointerTargetsForAutomation() const {
    return canvas.inspectPointerTargetsForAutomation();
}

var NodeWorkspace::inspectOpenGLDiagnosticsForAutomation() const {
    return canvas.inspectOpenGLDiagnosticsForAutomation();
}

void NodeWorkspace::resized() {
    canvas.setBounds(getLocalBounds());
}

}

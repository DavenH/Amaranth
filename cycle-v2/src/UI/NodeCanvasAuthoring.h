#pragma once

#include <JuceHeader.h>

#include "NodeEditorHost.h"
#include "TransformCompactEditor.h"
#include "VoiceContextCompactEditor.h"
#include "../Graph/GraphCommandDispatcher.h"
#include "../Runtime/GraphPresentationModel.h"

namespace CycleV2 {

struct NodeCanvasAuthoringSession {
    String selectedNodeId;
    String expandedNodeId;
    String statusMessage;
    int selectedEdgeIndex { -1 };
    int spliceTargetEdgeIndex { -1 };
};

struct NodeCanvasAuthoringEffects {
    bool repaintRequested {};
    bool editorBindingChanged {};
    bool resetInteraction {};
};

struct NodeCanvasAuthoringResult {
    bool handled {};
    bool succeeded {};
    bool graphChanged {};
    GraphEditCode graphEditCode { GraphEditCode::Connected };
    String nodeId;
    NodeCanvasAuthoringEffects effects;
};

class NodeCanvasAuthoring {
public:
    enum class OperationPortLayout {
        Side,
        Uptack,
        Vertical,
        Tee
    };

    NodeCanvasAuthoring(
            GraphDocument& document,
            GraphCommandDispatcher& commands,
            GraphPresentationModel& presentation,
            NodeEditorCommands& editorCommands);

    const NodeCanvasAuthoringSession& session() const { return authoringSession; }
    NodeCanvasAuthoringSession& interactionSession() { return authoringSession; }
    void setSession(NodeCanvasAuthoringSession session);
    void selectNode(const String& nodeId);
    void selectEdge(int edgeIndex);
    bool clearSelection();

    NodeCanvasAuthoringResult saveGraph(const File& file);
    NodeCanvasAuthoringResult loadGraph(const File& file);
    NodeCanvasAuthoringResult saveSnapshot(const File& file);
    NodeCanvasAuthoringResult loadSnapshot(const File& file);
    NodeCanvasAuthoringResult undo();
    NodeCanvasAuthoringResult redo();
    NodeCanvasAuthoringResult restoreGraphXml(const String& xml, const String& statusMessage);

    NodeCanvasAuthoringResult openEditor(const String& nodeId);
    NodeCanvasAuthoringResult addNode(NodeKind kind, Point<float> position);
    NodeCanvasAuthoringResult moveNode(const String& nodeId, Point<float> position);
    void beginNodeMoveGesture();
    bool resizeNodeDuringGesture(const String& nodeId, Rectangle<float> bounds);
    void commitNodeMoveGesture();
    NodeCanvasAuthoringResult connectPorts(
            const PortAddress& source,
            const PortAddress& destination);
    NodeCanvasAuthoringResult deleteNode(const String& nodeId);
    NodeCanvasAuthoringResult deleteEdge(int edgeIndex);
    NodeCanvasAuthoringResult toggleSignalProbe(int edgeIndex, float tapPosition);
    NodeCanvasAuthoringResult removeSignalProbe(const String& probeId);
    NodeCanvasAuthoringResult reattachSignalProbe(
            const String& probeId,
            int edgeIndex,
            float tapPosition);
    NodeCanvasAuthoringResult setNodeParameter(
            const String& nodeId,
            const String& parameterId,
            const String& label,
            const String& value);
    bool getNodeParameter(
            const String& nodeId,
            const String& parameterId,
            String& value) const;

    NodeCanvasAuthoringResult setTrimeshMorph(
            const String& nodeId,
            const String& axis,
            float value);
    NodeCanvasAuthoringResult setTrimeshPrimaryAxis(
            const String& nodeId,
            const String& axis);
    NodeCanvasAuthoringResult toggleTrimeshLink(
            const String& nodeId,
            const String& axis);
    NodeCanvasAuthoringResult selectTrimeshVertex(
            const String& nodeId,
            int vertexIndex);
    NodeCanvasAuthoringResult setTrimeshVertexParameter(
            const String& nodeId,
            const String& parameterId,
            float value);

    NodeCanvasAuthoringResult spliceSelectedNodeIntoEdge(int edgeIndex);
    NodeCanvasAuthoringResult cycleOperationPortLayout(const String& nodeId);
    NodeCanvasAuthoringResult cycleMeshOutputSide(const String& nodeId);
    NodeCanvasAuthoringResult cycleVoiceDomain(const String& nodeId);
    NodeCanvasAuthoringResult applyVoiceContextEdit(
            const String& nodeId,
            const VoiceContextEdit& edit);
    NodeCanvasAuthoringResult setTransformMode(
            const String& nodeId,
            TransformMode mode);

    static OperationPortLayout operationPortLayout(const Node& node);
    static OperationPortLayout nextOperationPortLayout(OperationPortLayout layout);
    static PortSide nextOutputSide(const Node& node);

private:
    NodeCanvasAuthoringResult graphEditResult(
            const GraphEditResult& edit,
            const String& statusMessage,
            const String& selectedNodeId,
            NodeCanvasAuthoringEffects effects = {});
    NodeCanvasAuthoringResult handledResult(
            bool succeeded,
            const String& statusMessage,
            NodeCanvasAuthoringEffects effects = {});
    NodeCanvasAuthoringResult restoredDocumentResult(const String& statusMessage);
    NodeCanvasAuthoringResult setVoiceContextParameter(
            const String& nodeId,
            const String& parameterId,
            const String& label,
            const String& value,
            const String& statusMessage);
    void refreshPresentation();
    void clearDocumentSelection();
    void spaceNodesAfterSplice(
            const String& upstreamNodeId,
            const String& insertedNodeId,
            const String& downstreamNodeId);
    const Node* findNode(const String& nodeId) const;

    GraphDocument& document;
    GraphCommandDispatcher& commands;
    GraphPresentationModel& presentation;
    NodeEditorCommands& editorCommands;
    NodeCanvasAuthoringSession authoringSession;
};

}

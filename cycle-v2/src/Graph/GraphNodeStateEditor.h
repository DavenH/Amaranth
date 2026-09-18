#pragma once

#include "Graph/GraphEditTypes.h"

namespace CycleV2 {

class GraphNodeStateEditor {
public:
    GraphEditResult setNodeParameter(
            NodeGraph& graph,
            const String& nodeId,
            const String& parameterId,
            const String& label,
            const String& value) const;
    GraphEditResult setNodeParametersAtomic(
            NodeGraph& graph,
            const String& nodeId,
            const std::vector<NodeParameter>& parameters) const;
    GraphEditResult replaceNodeModel(
            NodeGraph& graph,
            const String& nodeId,
            uint64_t expectedRevision,
            NodeModelStatePtr model) const;
    GraphEditResult replaceTransientNodeModel(
            NodeGraph& graph,
            const String& nodeId,
            uint64_t expectedRevision,
            NodeModelStatePtr model) const;
    GraphEditResult setNodeEditorState(
            NodeGraph& graph,
            const String& nodeId,
            var editorState) const;
    GraphEditResult setNodeAudioResource(
            NodeGraph& graph,
            NodeAudioResourceEdit edit) const;
    GraphEditResult removeNodeAudioResource(
            NodeGraph& graph,
            const String& nodeId) const;

private:
    Node* findMutableNode(NodeGraph& graph, const String& nodeId) const;
};

}

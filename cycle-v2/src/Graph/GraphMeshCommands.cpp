#include <algorithm>

#include "Graph/GraphCommandDispatcher.h"

#include "Graph/GraphNodeStateEditor.h"
#include "Graph/GraphMeshGuideRemap.h"
#include "Graph/MeshDimensionality.h"

namespace CycleV2 {

GraphEditResult GraphCommandDispatcher::setMeshDimensions(const String& nodeId, int count) {
    const Node* node = editingGraph().findNode(nodeId);
    if (node == nullptr || !MeshDimensionality::applies(node->kind)) {
        return { GraphEditCode::WrongNodeKind, nodeId, {} };
    }
    if (count < 0 || count > 3) {
        return { GraphEditCode::InvalidParameterValue, nodeId, {} };
    }
    const auto before = MeshDimensionality::dimensions(*node);
    if (before.axisCount() == count) {
        GraphEditResult result;
        result.changed = false;
        return result;
    }
    const auto retained = MeshDimensionality::dimensions(node->kind, count);
    const bool projects = (before.cornerMask() & ~retained.cornerMask()) != 0;
    std::vector<int> retainedCubeIndices;
    auto model = projects ? MeshDimensionality::projectModel(
            *node, count, MeshDimensionality::position(*node), &editingGraph(), &retainedCubeIndices) : node->model;
    if (model == nullptr) {
        return { GraphEditCode::InvalidTypedSnapshot, nodeId, {} };
    }
    return applyIncremental(
            [&](GraphDeltaBuilder& delta, const NodeGraph& graph) {
                delta.captureNodeModel(graph, nodeId);
                delta.captureNodeEditorState(graph, nodeId);
                delta.captureNodeParameter(graph, nodeId, "dimensionCount");
                delta.captureNodeParameter(graph, nodeId, "primaryAxis");
            },
            [&](NodeGraph& graph) {
                GraphNodeStateEditor editor;
                auto result = editor.setNodeParameter(
                        graph, nodeId, "dimensionCount", "Dimensions", String(count));
                if (projects) {
                    graph.replaceNodeModel(nodeId, model);
                    result.changes.modelChanged = true;
                    if (node->kind == NodeKind::TrilinearMesh) {
                        auto state = node->editorState.clone();
                        if (state.getDynamicObject() == nullptr) {
                            state = var(new DynamicObject());
                        }
                        state.getDynamicObject()->setProperty("selectedVertexId", -1);
                        graph.replaceNodeEditorState(nodeId, state);
                        result.changes.editorStateChanged = true;
                    }
                    remapMeshGuideAssignments(graph, nodeId, retainedCubeIndices, result, &retained);
                }
                const String primary = NodeParameterMap(*node).stringValue("primaryAxis", "yellow");
                if (node->kind == NodeKind::TrilinearMesh && count > 0
                        && !retained.active(MeshDimensionality::axisForPort(primary))) {
                    const auto axisChange = editor.setNodeParameter(
                            graph, nodeId, "primaryAxis", "Primary Axis", "yellow");
                    result.changes.parameterImpacts = result.changes.parameterImpacts | axisChange.changes.parameterImpacts;
                }
                return result;
            });
}

}

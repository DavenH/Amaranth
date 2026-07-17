#pragma once

#include <JuceHeader.h>

#include "../Graph/GraphCompiler.h"
#include "../Graph/GraphEditor.h"
#include "../Graph/GraphValidator.h"
#include "../Graph/NodeGraph.h"
#include "../Nodes/Trimesh/TrimeshRenderProfile.h"
#include "../Runtime/GraphPreviewExecutor.h"
#include "../Runtime/GraphRuntime.h"

namespace CycleV2 {

class NodeCanvasQueryModel {
public:
    NodeCanvasQueryModel(
            const NodeGraph& graph,
            const GraphCompileResult& compileResult,
            const RuntimeProcessTrace& runtimeTrace,
            const GraphPreviewResult& previewResult);

    const Node* findNode(const String& id) const;
    const Node* findNodeAt(Point<float> worldPosition) const;
    const Port* findPort(const Node& node, const String& portId, bool input) const;
    const RuntimeNodeTrace* findRuntimeTrace(const String& nodeId) const;
    const NodePreviewResult* findPreviewResult(const String& nodeId) const;

    PortDomain displayDomainForEdge(const Edge& edge) const;
    PortDomain displayDomainForNodeOutput(const Node& node, const String& portId) const;
    TrimeshRenderProfile renderProfileForNodeOutput(const Node& node, const String& portId) const;
    bool edgeHasValidationIssue(const Edge& edge) const;
    GraphValidationIssue validationIssueForEdge(const Edge& edge) const;
    int executionIndexForNode(const String& nodeId) const;
    int attachmentCount() const;

    String hoverTextForPort(const PortAddress& address) const;
    String hoverTextForNode(const Node& node) const;
    String hoverTextForEdge(const Edge& edge) const;

private:
    const NodeGraph& graph;
    const GraphCompileResult& compileResult;
    const RuntimeProcessTrace& runtimeTrace;
    const GraphPreviewResult& previewResult;
};

}

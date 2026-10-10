#pragma once

#include <JuceHeader.h>

#include "Graph/GraphCompiler.h"
#include "Graph/GraphEditTypes.h"
#include "Graph/GraphValidator.h"
#include "Graph/NodeGraph.h"
#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Runtime/GraphPresentationFacts.h"
#include "Runtime/GraphPresentationSnapshot.h"
#include "Runtime/GraphRuntime.h"

namespace CycleV2 {

class NodeCanvasQueryModel {
public:
    NodeCanvasQueryModel(
            const NodeGraph& graph,
            const GraphPresentationSnapshot& snapshot);

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

    const GraphPresentationFacts& presentationFacts() const;

private:
    const NodeGraph& graph;
    const GraphPresentationSnapshot* snapshot {};
    mutable std::shared_ptr<const GraphPresentationFacts> fallbackFacts;
};

}

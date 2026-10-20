#pragma once

#include <Curve/Mesh/MorphDimensions.h>
#include "Graph/NodeGraph.h"
#include "Graph/NodeParameterMap.h"

namespace CycleV2 {

class MeshDimensionality {
public:
    static NodeModelStatePtr projectModel(
            const Node& node, int count, const MorphPosition& position,
            const NodeGraph* graph = nullptr, std::vector<int>* retainedCubeIndices = nullptr);

    static bool applies(NodeKind kind) {
        return kind == NodeKind::TrilinearMesh || kind == NodeKind::Envelope;
    }

    static MorphDimensions dimensions(const Node& node) {
        return dimensions(node.kind, NodeParameterMap(node).intValue("dimensionCount", 0));
    }

    static MorphDimensions dimensions(NodeKind kind, int count) {
        return kind == NodeKind::TrilinearMesh ? MorphDimensions::trimesh(count) : MorphDimensions(count);
    }

    static int axisForPort(const String& port) {
        return port == "yellow" || port == "scratch" ? Vertex::Time
                : port == "red" ? Vertex::Red : port == "blue" ? Vertex::Blue : -1;
    }

    static bool inputActive(const Node& node, const String& port) {
        const int axis = axisForPort(port);
        return !applies(node.kind) || axis < 0 || dimensions(node).active(axis);
    }

    static MorphPosition position(const Node& node) {
        const NodeParameterMap parameters(node);
        return { parameters.floatValue("yellow", 0.f),
                parameters.floatValue("red", 0.5f), parameters.floatValue("blue", node.kind == NodeKind::Envelope ? 0.5f : 0.f) };
    }
};

}

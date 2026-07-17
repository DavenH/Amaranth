#include "FlatCurvePreparation.h"

#include "CurveNodeModels.h"

#include <Curve/Curve.h>
#include <Curve/Mesh/Vertex.h>
#include <Inter/Dimensions.h>

namespace CycleV2 {

FlatCurvePreparation::FlatCurvePreparation(
        const String& name,
        NodeKind nodeKind,
        const std::vector<NodeParameter>& parametersToPrepare,
        FXRasterizer::ScalingType scaling) :
        kind        (nodeKind)
    ,   parameters  (parametersToPrepare)
    ,   mesh        (name + "Mesh")
    ,   rasterizer  (nullptr, name + "Rasterizer") {
    rasterizer.setDims(Dimensions(Vertex::Phase, Vertex::Amp));
    rasterizer.setScalingMode(scaling);
    rasterizer.setMesh(&mesh);
}

FlatCurvePreparation::~FlatCurvePreparation() {
    mesh.destroy();
}

bool FlatCurvePreparation::prepare() {
    const auto vertices = CurveNodeModelCodec::flatVerticesFromParameters(parameters, kind);
    if (vertices.empty()) {
        return false;
    }

    for (const auto& state : vertices) {
        auto* vertex = new Vertex(state.x, state.y);
        vertex->values[Vertex::Curve] = state.curve;

        if (state.curve >= 1.f) {
            vertex->setMaxSharpness();
        }

        mesh.addVertex(vertex);
    }

    if (Curve::table == nullptr) {
        Curve::calcTable();
    }

    rasterizer.renderWaveformOnly();
    return rasterizer.canRasterizeWaveform();
}

}

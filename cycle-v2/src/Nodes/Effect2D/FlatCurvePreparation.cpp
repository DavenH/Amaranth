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
        NodeModelStatePtr modelToPrepare,
        FXRasterizer::ScalingType scaling) :
        kind        (nodeKind)
    ,   parameters  (parametersToPrepare)
    ,   model       (std::move(modelToPrepare))
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
    if (model == nullptr) {
        model = CurveNodeDomainCodec(kind).createDefault();
    }
    const auto typedModel = std::dynamic_pointer_cast<const CurveNodeModelState>(model);
    const FlatCurveModel* curve = typedModel != nullptr ? typedModel->flatCurve() : nullptr;
    if (curve == nullptr) {
        return false;
    }
    std::vector<Effect2DVertexState> vertices;
    vertices.reserve(curve->getVertices().size());
    for (const auto& vertex : curve->getVertices()) {
        vertices.push_back({ vertex.x, vertex.y, vertex.curve });
    }
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

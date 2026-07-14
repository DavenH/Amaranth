#include "CurveNodeModels.h"

#include "../Envelope/EnvelopeMeshState.h"

#include <Curve/Mesh/Vertex.h>

#include <algorithm>
#include <cmath>

namespace CycleV2 {

namespace {

String parameterValue(const std::vector<NodeParameter>& parameters, const String& id) {
    const auto found = std::find_if(parameters.begin(), parameters.end(), [&](const auto& parameter) {
        return parameter.id == id;
    });
    return found != parameters.end() ? found->value : String();
}

}

FlatCurveModel::FlatCurveModel(String name) :
        mesh(std::move(name)) {}

FlatCurveModel::~FlatCurveModel() {
    mesh.destroy();
}

bool FlatCurveModel::replaceVertices(std::vector<FlatCurveVertex> nextVertices) {
    if (!validate(nextVertices)) {
        return false;
    }

    std::sort(nextVertices.begin(), nextVertices.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.x < rhs.x;
    });
    const auto selected = selection;
    vertices = std::move(nextVertices);
    if (selected.has_value()) {
        selectVertex(*selected);
    }
    rebuildMesh();
    ++modelRevision;
    return true;
}

bool FlatCurveModel::selectVertex(uint64_t vertexId) {
    const auto found = std::find_if(vertices.begin(), vertices.end(), [&](const auto& vertex) {
        return vertex.id == vertexId;
    });
    selection = found != vertices.end() ? std::optional<uint64_t>(vertexId) : std::nullopt;
    return selection.has_value();
}

bool FlatCurveModel::loadSnapshot(const String& serialized) {
    const var root = JSON::parse(serialized);
    const auto* object = root.getDynamicObject();
    if (object == nullptr || (int) object->getProperty("version") != currentVersion
            || object->getProperty("type").toString() != "flatCurve") {
        return false;
    }

    const auto* array = object->getProperty("vertices").getArray();
    if (array == nullptr) {
        return false;
    }

    std::vector<FlatCurveVertex> nextVertices;
    nextVertices.reserve((size_t) array->size());
    for (const auto& item : *array) {
        const auto* vertex = item.getDynamicObject();
        if (vertex == nullptr) {
            return false;
        }
        nextVertices.push_back({
                (uint64_t) (int64) vertex->getProperty("id"),
                (float) vertex->getProperty("x"),
                (float) vertex->getProperty("y"),
                (float) vertex->getProperty("curve")
        });
    }

    if (!replaceVertices(std::move(nextVertices))) {
        return false;
    }

    const int64 selected = object->getProperty("selection");
    selection = std::nullopt;
    if (selected >= 0) {
        selectVertex((uint64_t) selected);
    }
    modelRevision = jmax<uint64_t>(1, (uint64_t) (int64) object->getProperty("revision"));
    return true;
}

bool FlatCurveModel::adoptEditedVertices(
        const std::vector<Effect2DVertexState>& editedVertices) {
    std::vector<FlatCurveVertex> nextVertices;
    nextVertices.reserve(editedVertices.size());
    uint64_t nextIdentity = 1;
    for (const auto& vertex : vertices) {
        nextIdentity = jmax(nextIdentity, vertex.id + 1);
    }

    std::vector<bool> matched(vertices.size(), false);
    const auto equal = [](float lhs, float rhs) {
        return std::abs(lhs - rhs) <= 0.000001f;
    };
    for (const auto& edited : editedVertices) {
        uint64_t identity {};
        for (size_t i = 0; i < vertices.size(); ++i) {
            if (!matched[i]
                    && equal(vertices[i].x, edited.x)
                    && equal(vertices[i].y, edited.y)
                    && equal(vertices[i].curve, edited.curve)) {
                identity = vertices[i].id;
                matched[i] = true;
                break;
            }
        }
        nextVertices.push_back({ identity != 0 ? identity : nextIdentity++, edited.x, edited.y, edited.curve });
    }
    return replaceVertices(std::move(nextVertices));
}

String FlatCurveModel::snapshot() const {
    auto root = std::make_unique<DynamicObject>();
    root->setProperty("version", currentVersion);
    root->setProperty("type", "flatCurve");
    root->setProperty("revision", (int64) modelRevision);
    root->setProperty("selection", selection.has_value() ? (int64) *selection : -1);
    Array<var> encodedVertices;
    for (const auto& vertex : vertices) {
        auto encoded = std::make_unique<DynamicObject>();
        encoded->setProperty("id", (int64) vertex.id);
        encoded->setProperty("x", vertex.x);
        encoded->setProperty("y", vertex.y);
        encoded->setProperty("curve", vertex.curve);
        encodedVertices.add(var(encoded.release()));
    }
    root->setProperty("vertices", var(encodedVertices));
    return JSON::toString(var(root.release()), false);
}

bool FlatCurveModel::validate(const std::vector<FlatCurveVertex>& verticesToValidate) {
    if (verticesToValidate.size() < 2) {
        return false;
    }

    std::vector<uint64_t> identities;
    identities.reserve(verticesToValidate.size());
    for (const auto& vertex : verticesToValidate) {
        if (vertex.id == 0 || !std::isfinite(vertex.x) || !std::isfinite(vertex.y)
                || !std::isfinite(vertex.curve) || vertex.x < 0.f || vertex.x > 1.f
                || vertex.y < 0.f || vertex.y > 1.f || vertex.curve < 0.f || vertex.curve > 1.f) {
            return false;
        }
        identities.push_back(vertex.id);
    }
    std::sort(identities.begin(), identities.end());
    return std::adjacent_find(identities.begin(), identities.end()) == identities.end();
}

void FlatCurveModel::rebuildMesh() {
    mesh.destroy();
    for (const auto& state : vertices) {
        auto* vertex = new Vertex(state.x, state.y);
        vertex->values[Vertex::Curve] = state.curve;
        if (state.curve >= 1.f) {
            vertex->setMaxSharpness();
        }
        mesh.addVertex(vertex);
    }
}

EnvelopeNodeModel::EnvelopeNodeModel() :
        mesh("CycleV2EnvelopeNodeModel") {
    applyEnvelopePayload(EnvelopeMeshState::defaultSnapshot());
}

EnvelopeNodeModel::~EnvelopeNodeModel() {
    mesh.destroy();
}

bool EnvelopeNodeModel::loadSnapshot(const String& serialized) {
    const var root = JSON::parse(serialized);
    const auto* object = root.getDynamicObject();
    if (object == nullptr || (int) object->getProperty("version") != currentVersion
            || object->getProperty("type").toString() != "envelope") {
        return false;
    }

    if (!applyEnvelopePayload(object->getProperty("payload").toString())) {
        return false;
    }
    logarithmic = (bool) object->getProperty("logarithmic");
    red = jlimit(0.f, 1.f, (float) object->getProperty("red"));
    blue = jlimit(0.f, 1.f, (float) object->getProperty("blue"));
    redLinked = (bool) object->getProperty("redLinked");
    blueLinked = (bool) object->getProperty("blueLinked");
    selection = (int) object->getProperty("selection");
    if (selection >= mesh.getNumCubes()) {
        selection = -1;
    }
    modelRevision = jmax<uint64_t>(1, (uint64_t) (int64) object->getProperty("revision"));
    return true;
}

bool EnvelopeNodeModel::syncFromNode(const Node& node) {
    const String typed = parameterValueForNode(node, CurveNodeModelCodec::snapshotParameterId(), {});
    const bool loaded = loadSnapshot(typed);
    logarithmic = parameterValueForNode(node, "logarithmic", "0").getIntValue() != 0;
    red = jlimit(0.f, 1.f, parameterValueForNode(node, "red", "0.5").getFloatValue());
    blue = jlimit(0.f, 1.f, parameterValueForNode(node, "blue", "0.5").getFloatValue());
    return loaded;
}

String EnvelopeNodeModel::snapshot() const {
    auto root = std::make_unique<DynamicObject>();
    root->setProperty("version", currentVersion);
    root->setProperty("type", "envelope");
    root->setProperty("revision", (int64) modelRevision);
    root->setProperty("payload", EnvelopeMeshState::serialize(mesh));
    root->setProperty("logarithmic", logarithmic);
    root->setProperty("red", red);
    root->setProperty("blue", blue);
    root->setProperty("redLinked", redLinked);
    root->setProperty("blueLinked", blueLinked);
    root->setProperty("selection", selection);
    return JSON::toString(var(root.release()), false);
}

bool EnvelopeNodeModel::selectCube(int cubeIndex) {
    if (cubeIndex < 0 || cubeIndex >= mesh.getNumCubes()) {
        return false;
    }
    selection = cubeIndex;
    return true;
}

bool EnvelopeNodeModel::applyEnvelopePayload(const String& payload) {
    EnvelopeMesh validated("CycleV2EnvelopeValidation");
    if (!EnvelopeMeshState::apply(payload, validated)) {
        validated.destroy();
        return false;
    }
    validated.destroy();

    mesh.destroy();
    if (!EnvelopeMeshState::apply(payload, mesh)) {
        return false;
    }
    ++modelRevision;
    return true;
}

void WaveshaperNodeModel::syncFromNode(const Node& node) {
    enabled = parameterValueForNode(node, "enabled", "1").getIntValue() != 0;
    preGain = jlimit(0.f, 1.f, parameterValueForNode(node, "pre", "0.5").getFloatValue());
    postGain = jlimit(0.f, 1.f, parameterValueForNode(node, "post", "0.5").getFloatValue());
    oversampling = jmax(1, parameterValueForNode(node, "aaFactor", "1").getIntValue());
    const String typed = parameterValueForNode(node, CurveNodeModelCodec::snapshotParameterId(), {});
    curve.loadSnapshot(typed);
}

void ImpulseResponseNodeModel::syncFromNode(const Node& node) {
    enabled = parameterValueForNode(node, "enabled", "1").getIntValue() != 0;
    size = jlimit(0.f, 1.f, parameterValueForNode(node, "size", "0.5").getFloatValue());
    postGain = jlimit(0.f, 1.f, parameterValueForNode(node, "post", "0.5").getFloatValue());
    highPass = jlimit(0.f, 1.f, parameterValueForNode(node, "highPass", "0.5").getFloatValue());
    const String typed = parameterValueForNode(node, CurveNodeModelCodec::snapshotParameterId(), {});
    curve.loadSnapshot(typed);
}

void GuideCurveNodeModel::syncFromNode(const Node& node) {
    enabled = parameterValueForNode(node, "enabled", "1").getIntValue() != 0;
    noise = jlimit(0.f, 1.f, parameterValueForNode(node, "noise", "0.5").getFloatValue());
    dcOffset = jlimit(0.f, 1.f, parameterValueForNode(node, "dcOffset", "0.5").getFloatValue());
    phase = jlimit(0.f, 1.f, parameterValueForNode(node, "phase", "0.5").getFloatValue());
    const String typed = parameterValueForNode(node, CurveNodeModelCodec::snapshotParameterId(), {});
    curve.loadSnapshot(typed);
}

String CurveNodeModelCodec::snapshotParameterId() {
    return "curve.modelSnapshot";
}

String CurveNodeModelCodec::revisionParameterId() {
    return "curve.modelRevision";
}

String CurveNodeModelCodec::defaultSnapshot(NodeKind kind) {
    if (kind == NodeKind::Envelope) {
        EnvelopeNodeModel model;
        model.setPublicationRevision(1);
        return model.snapshot();
    }

    std::vector<FlatCurveVertex> vertices;
    if (kind == NodeKind::GuideCurve) {
        vertices = {
                { 1, 0.05f, 0.5f, 1.f },
                { 2, 0.34f, 0.64f, 0.4f },
                { 3, 0.62f, 0.36f, 0.4f },
                { 4, 0.95f, 0.5f, 1.f }
        };
    } else if (kind == NodeKind::ImpulseResponse) {
        constexpr float padding = 0.0625f;
        vertices = {
                { 1, padding * 0.5f, 0.5f, 0.f },
                { 2, padding - 0.001f, 0.5f, 0.f },
                { 3, padding + 0.001f, 0.5f, 0.f },
                { 4, padding + 0.003f, 0.5f, 0.f },
                { 5, padding + 0.005f, 1.f, 0.f },
                { 6, padding + 0.010f, 0.1313f, 0.f },
                { 7, padding + 0.1f, 0.6f, 0.f },
                { 8, padding + 0.15f, 0.5f, 0.f },
                { 9, padding + 0.2f, 0.5f, 0.f },
                { 10, 1.f, 0.5f, 0.f }
        };
    } else if (kind == NodeKind::Waveshaper) {
        constexpr float padding = 0.125f;
        vertices = {
                { 1, padding * 0.5f, padding * 0.5f, 1.f },
                { 2, padding, padding, 1.f },
                { 3, 1.f - padding, 1.f - padding, 1.f },
                { 4, 1.f - padding * 0.5f, 1.f - padding * 0.5f, 1.f }
        };
    } else {
        return {};
    }

    FlatCurveModel model;
    if (!model.replaceVertices(std::move(vertices))) {
        return {};
    }
    model.setPublicationRevision(1);
    return model.snapshot();
}

String CurveNodeModelCodec::snapshotFromParameters(const std::vector<NodeParameter>& parameters) {
    return parameterValue(parameters, snapshotParameterId());
}

uint64_t CurveNodeModelCodec::revisionFromParameters(const std::vector<NodeParameter>& parameters) {
    return (uint64_t) parameterValue(parameters, revisionParameterId()).getLargeIntValue();
}

std::vector<Effect2DVertexState> CurveNodeModelCodec::flatVerticesFromParameters(
        const std::vector<NodeParameter>& parameters,
        NodeKind kind) {
    const String typedSnapshot = snapshotFromParameters(parameters);
    FlatCurveModel model;
    const String snapshot = typedSnapshot.isNotEmpty() ? typedSnapshot : defaultSnapshot(kind);
    if (model.loadSnapshot(snapshot)) {
        std::vector<Effect2DVertexState> result;
        result.reserve(model.getVertices().size());
        for (const auto& vertex : model.getVertices()) {
            result.push_back({ vertex.x, vertex.y, vertex.curve });
        }
        return result;
    }
    return {};
}

String CurveNodeModelCodec::envelopePayloadFromParameters(
        const std::vector<NodeParameter>& parameters) {
    const String typedSnapshot = snapshotFromParameters(parameters);
    EnvelopeNodeModel model;
    const String snapshot = typedSnapshot.isNotEmpty()
            ? typedSnapshot
            : defaultSnapshot(NodeKind::Envelope);
    if (model.loadSnapshot(snapshot)) {
        return EnvelopeMeshState::serialize(model.getMesh());
    }
    return {};
}

}

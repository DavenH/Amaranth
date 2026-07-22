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

    const auto selected = selection;
    vertices = std::move(nextVertices);
    if (selected.has_value()) {
        if (!selectVertex(selected)) {
            selection = std::nullopt;
        }
    }
    rebuildMesh();
    ++modelRevision;
    return true;
}

CurveEditResult FlatCurveModel::moveVertex(CurveVertexId vertexId, CurvePoint position) {
    const auto index = indicesByIdentity.find(vertexId);
    const auto meshVertex = verticesByIdentity.find(vertexId);
    if (index == indicesByIdentity.end() || meshVertex == verticesByIdentity.end()) {
        return { CurveEditCode::MissingIdentity, vertexId };
    }
    FlatCurveVertex& vertex = vertices[index->second];
    if (vertex.x == position.x && vertex.y == position.y) {
        return { CurveEditCode::NoChange, vertexId };
    }
    FlatCurveVertex edited = vertex;
    edited.x = position.x;
    edited.y = position.y;
    if (!validateVertex(edited)) {
        return { CurveEditCode::InvalidEdit, vertexId };
    }

    meshVertex->second->values[Vertex::Phase] = position.x;
    meshVertex->second->values[Vertex::Amp] = position.y;
    vertex = edited;
    ++modelRevision;
    return { CurveEditCode::Applied, vertexId };
}

CurveEditResult FlatCurveModel::setCurve(CurveVertexId vertexId, float curve) {
    const auto index = indicesByIdentity.find(vertexId);
    const auto meshVertex = verticesByIdentity.find(vertexId);
    if (index == indicesByIdentity.end() || meshVertex == verticesByIdentity.end()) {
        return { CurveEditCode::MissingIdentity, vertexId };
    }
    FlatCurveVertex& vertex = vertices[index->second];
    if (vertex.curve == curve) {
        return { CurveEditCode::NoChange, vertexId };
    }
    FlatCurveVertex edited = vertex;
    edited.curve = curve;
    if (!validateVertex(edited)) {
        return { CurveEditCode::InvalidEdit, vertexId };
    }

    meshVertex->second->values[Vertex::Curve] = curve;
    if (curve >= 1.f) {
        meshVertex->second->setMaxSharpness();
    }
    vertex = edited;
    ++modelRevision;
    return { CurveEditCode::Applied, vertexId };
}

CurveEditResult FlatCurveModel::insertVertex(CurvePoint position, float curve) {
    auto nextVertices = vertices;
    const CurveVertexId identity = nextIdentity();
    nextVertices.push_back({ identity, position.x, position.y, curve });
    if (!validate(nextVertices)) {
        return { CurveEditCode::InvalidEdit, identity };
    }

    auto* vertex = new Vertex(position.x, position.y);
    vertex->values[Vertex::Curve] = curve;
    if (curve >= 1.f) {
        vertex->setMaxSharpness();
    }
    mesh.addVertex(vertex);
    identitiesByVertex.emplace(vertex, identity);
    verticesByIdentity.emplace(identity, vertex);
    vertices = std::move(nextVertices);
    indicesByIdentity.emplace(identity, vertices.size() - 1);
    ++nextVertexIdentity;
    ++modelRevision;
    return { CurveEditCode::Applied, identity };
}

CurveEditResult FlatCurveModel::removeVertex(CurveVertexId vertexId) {
    const auto index = indicesByIdentity.find(vertexId);
    const auto meshVertex = verticesByIdentity.find(vertexId);
    if (index == indicesByIdentity.end() || meshVertex == verticesByIdentity.end()) {
        return { CurveEditCode::MissingIdentity, vertexId };
    }
    auto nextVertices = vertices;
    nextVertices.erase(nextVertices.begin()
            + static_cast<std::vector<FlatCurveVertex>::difference_type>(index->second));
    if (!validate(nextVertices)) {
        return { CurveEditCode::InvalidEdit, vertexId };
    }

    Vertex* removedVertex = meshVertex->second;
    if (!mesh.removeVert(removedVertex)) {
        return { CurveEditCode::MissingIdentity, vertexId };
    }
    identitiesByVertex.erase(removedVertex);
    verticesByIdentity.erase(vertexId);
    delete removedVertex;
    vertices = std::move(nextVertices);
    rebuildLookups();
    if (selection == vertexId) {
        selection = std::nullopt;
    }
    ++modelRevision;
    return { CurveEditCode::Applied, vertexId };
}

bool FlatCurveModel::selectVertex(std::optional<CurveVertexId> vertexId) {
    if (!vertexId.has_value()) {
        selection = std::nullopt;
        return true;
    }
    if (indicesByIdentity.find(*vertexId) == indicesByIdentity.end()) {
        return false;
    }
    selection = vertexId;
    return true;
}

Vertex* FlatCurveModel::vertexForIdentity(CurveVertexId vertexId) const {
    const auto found = verticesByIdentity.find(vertexId);
    return found != verticesByIdentity.end() ? found->second : nullptr;
}

Vertex* FlatCurveModel::selectedMeshVertex() const {
    return selection.has_value() ? vertexForIdentity(*selection) : nullptr;
}

bool FlatCurveModel::synchronizeFromMesh(Vertex* selectedVertex) {
    std::vector<FlatCurveVertex> nextVertices;
    nextVertices.reserve((size_t) mesh.getNumVerts());
    CurveVertexId identity = nextIdentity();
    std::unordered_map<Vertex*, CurveVertexId> nextIdentities;
    for (auto* vertex : mesh.getVerts()) {
        const auto existing = identitiesByVertex.find(vertex);
        const CurveVertexId vertexId = existing != identitiesByVertex.end() ? existing->second : identity++;
        nextIdentities.emplace(vertex, vertexId);
        nextVertices.push_back({
                vertexId,
                vertex->values[Vertex::Phase],
                vertex->values[Vertex::Amp],
                vertex->values[Vertex::Curve]
        });
    }
    if (!validate(nextVertices)) {
        rebuildMesh();
        return false;
    }
    const bool changed = nextVertices != vertices;
    vertices = std::move(nextVertices);
    identitiesByVertex = std::move(nextIdentities);
    rebuildLookups();
    const auto selected = identitiesByVertex.find(selectedVertex);
    selection = selected != identitiesByVertex.end()
            ? std::optional<CurveVertexId>(selected->second)
            : std::nullopt;
    if (changed) {
        ++modelRevision;
    }
    return true;
}

bool FlatCurveModel::loadSnapshot(const String& serialized) {
    return readJSON(JSON::parse(serialized));
}

bool FlatCurveModel::readJSON(const var& root) {
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
        selectVertex((CurveVertexId) selected);
    }
    modelRevision = jmax<uint64_t>(1, (uint64_t) (int64) object->getProperty("revision"));
    return true;
}

String FlatCurveModel::snapshot() const {
    return JSON::toString(writeJSON(), false);
}

var FlatCurveModel::writeJSON() const {
    auto root = std::make_unique<DynamicObject>();
    root->setProperty("version", currentVersion);
    root->setProperty("type", "flatCurve");
    root->setProperty("revision", (int64) modelRevision);
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
    return var(root.release());
}

bool FlatCurveModel::validate(const std::vector<FlatCurveVertex>& verticesToValidate) {
    if (verticesToValidate.size() < 2) {
        return false;
    }

    std::unordered_set<CurveVertexId> identities;
    identities.reserve(verticesToValidate.size());
    for (const auto& vertex : verticesToValidate) {
        if (!validateVertex(vertex)) {
            return false;
        }
        if (!identities.emplace(vertex.id).second) {
            return false;
        }
    }
    return true;
}

bool FlatCurveModel::validateVertex(const FlatCurveVertex& vertex) {
    return vertex.id != 0 && std::isfinite(vertex.x) && std::isfinite(vertex.y)
            && std::isfinite(vertex.curve) && vertex.x >= 0.f && vertex.x <= 1.f
            && vertex.y >= 0.f && vertex.y <= 1.f && vertex.curve >= 0.f && vertex.curve <= 1.f;
}

void FlatCurveModel::rebuildMesh() {
    mesh.destroy();
    identitiesByVertex.clear();
    verticesByIdentity.clear();
    for (const auto& state : vertices) {
        auto* vertex = new Vertex(state.x, state.y);
        vertex->values[Vertex::Curve] = state.curve;
        if (state.curve >= 1.f) {
            vertex->setMaxSharpness();
        }
        mesh.addVertex(vertex);
        identitiesByVertex.emplace(vertex, state.id);
        verticesByIdentity.emplace(state.id, vertex);
    }
    rebuildLookups();
}

void FlatCurveModel::rebuildLookups() {
    indicesByIdentity.clear();
    verticesByIdentity.clear();
    for (size_t i = 0; i < vertices.size(); ++i) {
        indicesByIdentity.emplace(vertices[i].id, i);
        nextVertexIdentity = jmax(nextVertexIdentity, vertices[i].id + 1);
    }
    for (const auto& entry : identitiesByVertex) {
        verticesByIdentity.emplace(entry.second, entry.first);
    }
}

CurveVertexId FlatCurveModel::nextIdentity() const {
    return nextVertexIdentity;
}

EnvelopeNodeModel::EnvelopeNodeModel() :
        mesh("CycleV2EnvelopeNodeModel") {
    applyEnvelopePayload(EnvelopeMeshState::defaultSnapshot());
}

EnvelopeNodeModel::~EnvelopeNodeModel() {
    mesh.destroy();
}

bool EnvelopeNodeModel::loadSnapshot(const String& serialized) {
    return readJSON(JSON::parse(serialized));
}

bool EnvelopeNodeModel::readJSON(const var& root) {
    const auto* object = root.getDynamicObject();
    if (object == nullptr || (int) object->getProperty("version") != currentVersion
            || object->getProperty("type").toString() != "envelope") {
        return false;
    }

    EnvelopeMesh validated("CycleV2EnvelopeSnapshotValidation");
    const var meshState = object->getProperty("mesh");
    if (!validated.readJSON(meshState)) {
        validated.destroy();
        return false;
    }

    const auto* encodedIds = object->getProperty("cubeIds").getArray();
    if (encodedIds == nullptr || encodedIds->size() != validated.getNumCubes()) {
        validated.destroy();
        return false;
    }
    std::vector<EnvelopeCubeId> nextIds;
    nextIds.reserve((size_t) encodedIds->size());
    for (const auto& encodedId : *encodedIds) {
        nextIds.push_back((EnvelopeCubeId) (int64) encodedId);
    }
    std::unordered_set<EnvelopeCubeId> uniqueIds;
    uniqueIds.reserve(nextIds.size());
    const bool idsValid = !nextIds.empty() && std::all_of(nextIds.begin(), nextIds.end(), [&](EnvelopeCubeId id) {
        return id != 0 && uniqueIds.emplace(id).second;
    });
    if (!idsValid) {
        validated.destroy();
        return false;
    }
    validated.destroy();

    mesh.destroy();
    if (!mesh.readJSON(meshState)) {
        return false;
    }
    cubeIds = std::move(nextIds);
    rebuildIdentityMap();
    committedPayload = JSON::toString(meshState, false);
    logarithmic = (bool) object->getProperty("logarithmic");
    red = jlimit(0.f, 1.f, (float) object->getProperty("red"));
    blue = jlimit(0.f, 1.f, (float) object->getProperty("blue"));
    redLinked = (bool) object->getProperty("redLinked");
    blueLinked = (bool) object->getProperty("blueLinked");
    const int64 selected = object->getProperty("selection");
    selection = std::nullopt;
    if (selected >= 0) {
        selectCube((EnvelopeCubeId) selected);
    }
    modelRevision = jmax<uint64_t>(1, (uint64_t) (int64) object->getProperty("revision"));
    return true;
}

bool EnvelopeNodeModel::syncFromNode(const Node& node) {
    const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    const bool loaded = typed != nullptr && readJSON(typed->domainJSON());
    logarithmic = parameterValueForNode(node, "logarithmic", "0").getIntValue() != 0;
    dynamicWhileLive = parameterValueForNode(node, "dynamic", "0").getIntValue() != 0;
    red = jlimit(0.f, 1.f, parameterValueForNode(node, "red", "0.5").getFloatValue());
    blue = jlimit(0.f, 1.f, parameterValueForNode(node, "blue", "0.5").getFloatValue());
    return loaded;
}

String EnvelopeNodeModel::snapshot() const {
    return JSON::toString(writeJSON(), false);
}

var EnvelopeNodeModel::writeJSON() const {
    auto root = std::make_unique<DynamicObject>();
    root->setProperty("version", currentVersion);
    root->setProperty("type", "envelope");
    root->setProperty("revision", (int64) modelRevision);
    root->setProperty("mesh", mesh.writeJSON());
    root->setProperty("logarithmic", logarithmic);
    root->setProperty("red", red);
    root->setProperty("blue", blue);
    root->setProperty("redLinked", redLinked);
    root->setProperty("blueLinked", blueLinked);
    Array<var> encodedIds;
    for (const auto cubeId : cubeIds) {
        encodedIds.add((int64) cubeId);
    }
    root->setProperty("cubeIds", var(encodedIds));
    return var(root.release());
}

bool EnvelopeNodeModel::selectCube(std::optional<EnvelopeCubeId> cubeId) {
    if (!cubeId.has_value()) {
        selection = std::nullopt;
        return true;
    }
    if (cubesByIdentity.find(*cubeId) == cubesByIdentity.end()) {
        return false;
    }
    selection = cubeId;
    return true;
}

VertCube* EnvelopeNodeModel::cubeForIdentity(EnvelopeCubeId cubeId) const {
    const auto found = cubesByIdentity.find(cubeId);
    return found != cubesByIdentity.end() ? found->second : nullptr;
}

VertCube* EnvelopeNodeModel::selectedMeshCube() const {
    return selection.has_value() ? cubeForIdentity(*selection) : nullptr;
}

bool EnvelopeNodeModel::synchronizeFromMesh(VertCube* selectedCube) {
    std::vector<EnvelopeCubeId> nextIds;
    nextIds.reserve((size_t) mesh.getNumCubes());
    auto nextIdentityByCube = identitiesByCube;
    EnvelopeCubeId identity = nextIdentity();
    for (auto* cube : mesh.getCubes()) {
        const auto existing = identitiesByCube.find(cube);
        const EnvelopeCubeId cubeId = existing != identitiesByCube.end() ? existing->second : identity++;
        nextIds.push_back(cubeId);
        nextIdentityByCube[cube] = cubeId;
    }
    std::unordered_set<VertCube*> liveCubes(mesh.getCubes().begin(), mesh.getCubes().end());
    for (auto it = nextIdentityByCube.begin(); it != nextIdentityByCube.end();) {
        if (liveCubes.find(it->first) == liveCubes.end()) {
            it = nextIdentityByCube.erase(it);
        } else {
            ++it;
        }
    }

    const String payload = EnvelopeMeshState::serialize(mesh);
    EnvelopeMesh validated("CycleV2EnvelopeEditValidation");
    if (!EnvelopeMeshState::apply(payload, validated)) {
        validated.destroy();
        mesh.destroy();
        EnvelopeMeshState::apply(committedPayload, mesh);
        rebuildIdentityMap();
        return false;
    }
    validated.destroy();
    const bool changed = nextIds != cubeIds || payload != committedPayload;
    cubeIds = std::move(nextIds);
    identitiesByCube = std::move(nextIdentityByCube);
    nextCubeIdentity = jmax(nextCubeIdentity, identity);
    rebuildIdentityMap();
    const auto selected = identitiesByCube.find(selectedCube);
    selection = selected != identitiesByCube.end()
            ? std::optional<EnvelopeCubeId>(selected->second)
            : std::nullopt;
    committedPayload = payload;
    if (changed) {
        ++modelRevision;
    }
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
    cubeIds.clear();
    EnvelopeCubeId identity = 1;
    for (size_t i = 0; i < mesh.getCubes().size(); ++i) {
        cubeIds.push_back(identity++);
    }
    rebuildIdentityMap();
    committedPayload = payload;
    selection = std::nullopt;
    ++modelRevision;
    return true;
}

EnvelopeCubeId EnvelopeNodeModel::nextIdentity() const {
    return nextCubeIdentity;
}

void EnvelopeNodeModel::rebuildIdentityMap() {
    identitiesByCube.clear();
    cubesByIdentity.clear();
    for (size_t i = 0; i < cubeIds.size(); ++i) {
        identitiesByCube.emplace(mesh.getCubes()[i], cubeIds[i]);
        cubesByIdentity.emplace(cubeIds[i], mesh.getCubes()[i]);
        nextCubeIdentity = jmax(nextCubeIdentity, cubeIds[i] + 1);
    }
}

void WaveshaperNodeModel::syncFromNode(const Node& node) {
    enabled = parameterValueForNode(node, "enabled", "1").getIntValue() != 0;
    preGain = jlimit(0.f, 1.f, parameterValueForNode(node, "pre", "0.5").getFloatValue());
    postGain = jlimit(0.f, 1.f, parameterValueForNode(node, "post", "0.5").getFloatValue());
    oversampling = jmax(1, parameterValueForNode(node, "aaFactor", "1").getIntValue());
    const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    if (typed != nullptr) {
        curve.readJSON(typed->domainJSON());
    }
}

void ImpulseResponseNodeModel::syncFromNode(const Node& node) {
    enabled = parameterValueForNode(node, "enabled", "1").getIntValue() != 0;
    size = jlimit(0.f, 1.f, parameterValueForNode(node, "size", "0.5").getFloatValue());
    postGain = jlimit(0.f, 1.f, parameterValueForNode(node, "post", "0.5").getFloatValue());
    highPass = jlimit(0.f, 1.f, parameterValueForNode(node, "highPass", "0.5").getFloatValue());
    const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    if (typed != nullptr) {
        curve.readJSON(typed->domainJSON());
    }
}

void GuideCurveNodeModel::syncFromNode(const Node& node) {
    enabled = parameterValueForNode(node, "enabled", "1").getIntValue() != 0;
    noise = jlimit(0.f, 1.f, parameterValueForNode(node, "noise", "0.5").getFloatValue());
    dcOffset = jlimit(0.f, 1.f, parameterValueForNode(node, "dcOffset", "0.5").getFloatValue());
    phase = jlimit(0.f, 1.f, parameterValueForNode(node, "phase", "0.5").getFloatValue());
    const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    if (typed != nullptr) {
        curve.readJSON(typed->domainJSON());
    }
}

static var defaultCurveModelState(NodeKind kind) {
    if (kind == NodeKind::Envelope) {
        EnvelopeNodeModel model;
        model.setPublicationRevision(1);
        return model.writeJSON();
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
    return model.writeJSON();
}

CurveNodeModelState::CurveNodeModelState(
        String schemaToUse,
        int versionToUse,
        uint64_t revisionToUse,
        var modelState,
        var editorStateToUse) :
        schema(std::move(schemaToUse))
    ,   version(versionToUse)
    ,   modelRevision(revisionToUse)
    ,   state(std::move(modelState))
    ,   editorState(std::move(editorStateToUse)) {}

String CurveNodeModelState::schemaId() const {
    return schema;
}

int CurveNodeModelState::schemaVersion() const {
    return version;
}

uint64_t CurveNodeModelState::revision() const {
    return modelRevision;
}

var CurveNodeModelState::writeJSON() const {
    auto result = std::make_unique<DynamicObject>();
    result->setProperty("schema", schema);
    result->setProperty("version", version);
    result->setProperty("revision", (int64) modelRevision);
    result->setProperty("state", state);
    return var(result.release());
}

bool CurveNodeModelState::equals(const NodeModelState& other) const {
    const auto* typed = dynamic_cast<const CurveNodeModelState*>(&other);
    return typed != nullptr
            && schema == typed->schema
            && version == typed->version
            && modelRevision == typed->modelRevision
            && JSON::toString(state, false) == JSON::toString(typed->state, false);
}

CurveNodeDomainCodec::CurveNodeDomainCodec(NodeKind kindToUse) : kind(kindToUse) {}

String CurveNodeDomainCodec::schemaId() const {
    return kind == NodeKind::Envelope ? "envelope" : "flatCurve";
}

int CurveNodeDomainCodec::currentVersion() const {
    return kind == NodeKind::Envelope
            ? EnvelopeNodeModel::currentVersion
            : FlatCurveModel::currentVersion;
}

NodeModelStatePtr CurveNodeDomainCodec::createDefault() const {
    const var state = defaultCurveModelState(kind);
    String error;
    auto wrapper = std::make_unique<DynamicObject>();
    wrapper->setProperty("schema", schemaId());
    wrapper->setProperty("version", currentVersion());
    wrapper->setProperty("revision", 1);
    wrapper->setProperty("state", state);
    return readJSON(var(wrapper.release()), error);
}

NodeModelStatePtr CurveNodeDomainCodec::readJSON(const var& value, String& error) const {
    const auto* object = value.getDynamicObject();
    if (object == nullptr || object->getProperty("schema").toString() != schemaId()) {
        error = "Unexpected node model schema";
        return nullptr;
    }
    if ((int) object->getProperty("version") != currentVersion()) {
        error = "Unsupported node model schema version";
        return nullptr;
    }
    const int64 revision = object->getProperty("revision");
    if (revision < 1) {
        error = "Node model revision must be positive";
        return nullptr;
    }

    const var state = object->getProperty("state");
    var canonicalState;
    if (kind == NodeKind::Envelope) {
        EnvelopeNodeModel model;
        if (model.readJSON(state) && model.revision() == (uint64_t) revision) {
            canonicalState = model.writeJSON();
        }
    } else {
        FlatCurveModel model;
        if (model.readJSON(state) && model.revision() == (uint64_t) revision) {
            canonicalState = model.writeJSON();
        }
    }
    if (canonicalState.isVoid()) {
        error = "Invalid structured node model state";
        return nullptr;
    }
    return std::make_shared<const CurveNodeModelState>(
            schemaId(), currentVersion(), (uint64_t) revision, std::move(canonicalState));
}

}

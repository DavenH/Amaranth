#include "Graph/MeshDimensionalityMigration.h"

#include "Graph/MeshDimensionality.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"

namespace CycleV2 {

TrimeshSemanticRepresentationMigrationResult MeshDimensionalityMigration::migrate(var& graph) const {
    TrimeshSemanticRepresentationMigrationResult result;
    auto* root = graph.getDynamicObject();
    const int version = root != nullptr ? (int) root->getProperty("formatVersion") : 9;
    if (root == nullptr || version == 9) {
        return result;
    }
    if (version != 7 && version != 8) {
        result.error = "Unsupported mesh dimensionality graph version";
        return result;
    }
    auto* nodes = root->getProperty("nodes").getArray();
    if (nodes == nullptr) {
        result.error = "Graph nodes must be an array";
        return result;
    }
    for (auto& value : *nodes) {
        auto* encoded = value.getDynamicObject();
        auto* parameters = encoded != nullptr
                ? encoded->getProperty("parameters").getDynamicObject() : nullptr;
        if (parameters == nullptr) {
            result.error = "Node parameters must be objects";
            return result;
        }
        const String kind = encoded->getProperty("kind").toString();
        if (version == 8) {
            // The first dimensional format used Red/Blue for Trimesh 2D.
            // Keep that authored variation available when adopting Yellow/Red.
            if (kind == "trilinearMesh" && parameters->getProperty("dimensionCount").toString() == "2") {
                parameters->setProperty("dimensionCount", 3);
            }
            continue;
        }
        if (kind == "trilinearMesh") {
            parameters->setProperty("dimensionCount", 3);
        } else if (kind == "envelope") {
            Node node;
            node.kind = NodeKind::Envelope;
            String error;
            const auto encodedModel = encoded->getProperty("model");
            const CurveNodeDomainCodec codec(node.kind);
            node.model = encodedModel.isVoid() ? codec.createDefault() : codec.readJSON(encodedModel, error);
            if (node.model == nullptr) {
                result.error = "Cannot migrate Envelope: " + error;
                return result;
            }
            const float red = parameters->hasProperty("red")
                    ? parameters->getProperty("red").toString().getFloatValue() : 0.5f;
            const float blue = parameters->hasProperty("blue")
                    ? parameters->getProperty("blue").toString().getFloatValue() : 0.5f;
            std::vector<int> retainedCells;
            auto model = MeshDimensionality::projectModel(node, 2, { 0.f, red, blue }, nullptr, &retainedCells);
            if (auto* assignments = root->getProperty("guideAssignments").getArray()) {
                for (int index = assignments->size() - 1; index >= 0; --index) {
                    auto* assignment = assignments->getReference(index).getDynamicObject();
                    if (assignment == nullptr || assignment->getProperty("targetNodeId") != encoded->getProperty("id")) {
                        continue;
                    }
                    auto* target = assignment->getProperty("target").getDynamicObject();
                    if (target == nullptr) {
                        continue;
                    }
                    const auto cell = std::find(retainedCells.begin(), retainedCells.end(), (int) target->getProperty("cubeIndex"));
                    if (cell == retainedCells.end()) {
                        assignments->remove(index);
                    } else {
                        target->setProperty("cubeIndex", (int) std::distance(retainedCells.begin(), cell));
                    }
                }
            }
            encoded->setProperty("model", model->writeJSON());
            parameters->setProperty("dimensionCount", 2);
            parameters->setProperty("yellow", 0.f);
        } else if (kind == "modulationTriple") {
            parameters->setProperty("envelopeYellowSource", "unmapped");
            for (const String axis : { String("Red"), String("Blue") }) {
                const String original = axis.toLowerCase();
                for (const String field : { String("Source"), String("Controller"), String("Constant") }) {
                    const String id = original + field;
                    if (parameters->hasProperty(id)) {
                        parameters->setProperty("envelope" + axis + field, parameters->getProperty(id));
                    }
                }
            }
        }
    }
    root->setProperty("formatVersion", 9);
    result.migrated = true;
    return result;
}

}

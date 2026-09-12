#include "Graph/GlobalAudioGraphMigration.h"

#include "Graph/GlobalAudioGraphRepresentationMigration.h"
#include "Graph/GraphSerializer.h"

namespace CycleV2 {

GlobalAudioGraphMigrationResult GlobalAudioGraphMigration::migrate(NodeGraph& graph) const {
    GlobalAudioGraphMigrationResult result;
    if (graph.findNode("voiceOutput") != nullptr
            && graph.findNode("globalInput") != nullptr) {
        return result;
    }

    GraphSerializer serializer;
    var encoded = serializer.writeJSON(graph);
    encoded.getDynamicObject()->setProperty(
            "formatVersion",
            graph.findNode("globalInput") != nullptr ? 5 : 4);
    const auto representationMigration =
            GlobalAudioGraphRepresentationMigration().migrate(encoded);
    if (!representationMigration.succeeded()) {
        result.error = representationMigration.error;
        return result;
    }

    const auto decoded = serializer.readJSON(encoded);
    if (!decoded.succeeded()) {
        result.error = decoded.issues.empty()
                ? "Migrated graph could not be decoded"
                : decoded.issues.front().message;
        return result;
    }

    graph = decoded.graph;
    result.migrated = representationMigration.migrated;
    result.globalNodeIds = representationMigration.globalNodeIds;
    return result;
}

}

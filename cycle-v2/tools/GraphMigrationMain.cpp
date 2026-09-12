#include <JuceHeader.h>

#include "Graph/GraphSerializer.h"
#include "Graph/GlobalAudioGraphRepresentationMigration.h"

#include <iostream>

using namespace CycleV2;
using namespace juce;

int main(int argc, char* argv[]) {
    const bool raw = argc == 4 && String(argv[1]) == "--raw";
    if ((!raw && argc != 3) || (raw && argc != 4)) {
        std::cerr << "Usage: CycleV2GraphMigrator [--raw] <source> <destination>\n";
        return 2;
    }

    const int sourceIndex = raw ? 2 : 1;
    const File source = File::getCurrentWorkingDirectory().getChildFile(argv[sourceIndex]);
    const File destination = File::getCurrentWorkingDirectory().getChildFile(argv[sourceIndex + 1]);
    if (raw) {
        var encoded;
        const Result parsed = JSON::parse(source.loadFileAsString(), encoded);
        const auto migrated = parsed.wasOk()
                ? GlobalAudioGraphRepresentationMigration().migrate(encoded)
                : GlobalAudioGraphRepresentationMigrationResult {
                        false,
                        parsed.getErrorMessage(),
                        {}
                };
        if (!migrated.succeeded()) {
            std::cerr << migrated.error << "\n";
            return 1;
        }
        if (!destination.replaceWithText(GraphSerializer().toJsonString(encoded))) {
            std::cerr << "Could not write migrated graph\n";
            return 1;
        }
        return 0;
    }
    const auto loaded = GraphSerializer().loadJsonString(source.loadFileAsString());
    if (!loaded.succeeded()) {
        for (const auto& issue : loaded.issues) {
            std::cerr << issue.message << "\n";
        }
        return 1;
    }
    if (!destination.replaceWithText(GraphSerializer().toJsonString(loaded.graph))) {
        std::cerr << "Could not write migrated graph\n";
        return 1;
    }
    return 0;
}

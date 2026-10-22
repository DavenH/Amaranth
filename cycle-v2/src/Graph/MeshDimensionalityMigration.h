#pragma once

#include "Graph/TrimeshSemanticRepresentationMigration.h"

namespace CycleV2 {

class MeshDimensionalityMigration {
public:
    TrimeshSemanticRepresentationMigrationResult migrate(juce::var& graph) const;
};

}

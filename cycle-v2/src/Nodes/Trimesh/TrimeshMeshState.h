#pragma once

#include <JuceHeader.h>

#include "../../Graph/NodeDefinition.h"

class Mesh;

namespace CycleV2 {

class TrimeshNodeModelState : public NodeModelState {
public:
    static std::shared_ptr<const TrimeshNodeModelState> copyOf(
            const Mesh& mesh,
            uint64_t revisionToUse);

    String schemaId() const override;
    int schemaVersion() const override;
    uint64_t revision() const override;
    var writeJSON() const override;
    bool equals(const NodeModelState& other) const override;

    const Mesh& mesh() const { return *meshState; }
    std::shared_ptr<const Mesh> sharedMesh() const { return meshState; }

private:
    friend class TrimeshNodeModelCodec;

    TrimeshNodeModelState(std::shared_ptr<Mesh> meshToUse, uint64_t revisionToUse);

    std::shared_ptr<const Mesh> meshState;
    uint64_t modelRevision {};
};

class TrimeshNodeModelCodec : public NodeModelCodec {
public:
    String schemaId() const override;
    int currentVersion() const override;
    NodeModelStatePtr createDefault() const override;
    NodeModelStatePtr readJSON(const var& value, String& error) const override;
};

}

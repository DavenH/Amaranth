#pragma once

#include "NodeGraph.h"

#include <vector>

namespace CycleV2 {

enum class GraphValidationCode {
    MissingSourceNode,
    MissingDestinationNode,
    MissingSourcePort,
    MissingDestinationPort,
    DomainMismatch,
    InvalidAttachmentSource,
    InvalidAttachmentDestination,
    ScratchPortRequiresAttachment,
    PitchRequiresVoiceAwareDestination
};

struct GraphValidationIssue {
    GraphValidationCode code {};
    String message;
};

class GraphValidator {
public:
    std::vector<GraphValidationIssue> validate(const NodeGraph& graph) const;
    bool isValid(const NodeGraph& graph) const;

private:
    bool isVoiceAwareDestination(const Port& port) const;
    bool domainsCompatible(const Port& source, const Port& dest) const;
};

}

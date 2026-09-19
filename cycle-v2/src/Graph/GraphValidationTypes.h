#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

enum class GraphValidationCode {
    MissingSourceNode,
    MissingDestinationNode,
    MissingSourcePort,
    MissingDestinationPort,
    DomainMismatch,
    ChannelLayoutMismatch,
    InvalidAttachmentSource,
    InvalidAttachmentDestination,
    ScratchPortRequiresAttachment,
    PitchRequiresVoiceAwareDestination,
    MixedOperationDomains,
    MissingRequiredNode,
    DuplicateSingletonNode,
    ProcessingScopeMismatch,
    ConflictingProcessingScope,
    GlobalNodeUnreachable,
    GlobalNodeCannotReachOutput,
    AmbiguousVoiceOutput,
    MissingVoiceContextAssignment,
    MultipleActiveVoiceContexts
};

struct GraphValidationIssue {
    GraphValidationCode code {};
    juce::String message;
    juce::String sourceNodeId;
    juce::String sourcePortId;
    juce::String destNodeId;
    juce::String destPortId;
};

}

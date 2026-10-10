#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <optional>

namespace CycleV2 {

struct NodeModelEnvelope {
    uint64_t revision {};
    juce::var payload;
};

class NodeModelEnvelopeCodec {
public:
    static juce::var write(
            const juce::String& schema,
            int version,
            uint64_t revision,
            const juce::Identifier& payloadProperty,
            juce::var payload);
    static std::optional<NodeModelEnvelope> read(
            const juce::var& value,
            const juce::String& expectedSchema,
            int expectedVersion,
            const juce::Identifier& payloadProperty,
            juce::String& error);
};

}

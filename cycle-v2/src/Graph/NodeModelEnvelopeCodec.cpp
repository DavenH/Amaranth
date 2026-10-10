#include "Graph/NodeModelEnvelopeCodec.h"

#include <utility>

namespace CycleV2 {

juce::var NodeModelEnvelopeCodec::write(
        const juce::String& schema,
        int version,
        uint64_t revision,
        const juce::Identifier& payloadProperty,
        juce::var payload) {
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("schema", schema);
    result->setProperty("version", version);
    result->setProperty("revision", (juce::int64) revision);
    result->setProperty(payloadProperty, std::move(payload));
    return juce::var(result.release());
}

std::optional<NodeModelEnvelope> NodeModelEnvelopeCodec::read(
        const juce::var& value,
        const juce::String& expectedSchema,
        int expectedVersion,
        const juce::Identifier& payloadProperty,
        juce::String& error) {
    const auto* object = value.getDynamicObject();
    if (object == nullptr
            || object->getProperty("schema").toString() != expectedSchema) {
        error = "Expected node model schema '" + expectedSchema + "'";
        return std::nullopt;
    }
    if ((int) object->getProperty("version") != expectedVersion) {
        error = "Unsupported node model schema version";
        return std::nullopt;
    }
    const juce::int64 revision = object->getProperty("revision");
    if (revision < 1) {
        error = "Node model revision must be positive";
        return std::nullopt;
    }

    return NodeModelEnvelope {
            (uint64_t) revision,
            object->getProperty(payloadProperty)
    };
}

}

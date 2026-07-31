#include "TrimeshGuideAttachmentTarget.h"

namespace CycleV2 {

const std::array<juce::String, TrimeshGuideAttachmentTarget::fieldCount>&
TrimeshGuideAttachmentTarget::fields() {
    static const std::array<juce::String, fieldCount> values {
            "time",
            "red",
            "blue",
            "phase",
            "amp",
            "curve"
    };

    return values;
}

bool TrimeshGuideAttachmentTarget::isValid() const {
    return (vertexIndex >= 0) != (cubeIndex >= 0) && fieldIndex() >= 0;
}

int TrimeshGuideAttachmentTarget::fieldIndex() const {
    const auto& values = fields();

    for (int i = 0; i < (int) values.size(); ++i) {
        if (values[(size_t) i] == field) {
            return i;
        }
    }

    return -1;
}

TrimeshGuideAttachmentTarget TrimeshGuideAttachmentTarget::parse(const juce::String& portId) {
    const bool cubeTarget = portId.startsWith("guide.cube.");
    const juce::String prefix = cubeTarget ? "guide.cube." : "guide.vertex.";
    if (!portId.startsWith(prefix)) {
        return {};
    }

    const juce::String suffix = portId.fromFirstOccurrenceOf(prefix, false, false);
    const juce::String targetIndexText = suffix.upToFirstOccurrenceOf(".", false, false);
    const juce::String fieldText = suffix.fromFirstOccurrenceOf(".", false, false);

    if (targetIndexText.isEmpty() || !targetIndexText.containsOnly("0123456789")) {
        return {};
    }

    TrimeshGuideAttachmentTarget target {
            cubeTarget ? -1 : targetIndexText.getIntValue(),
            fieldText
    };
    target.cubeIndex = cubeTarget ? targetIndexText.getIntValue() : -1;

    return target.isValid() ? target : TrimeshGuideAttachmentTarget {};
}

juce::String TrimeshGuideAttachmentTarget::portIdFor(
        int vertexIndex,
        const juce::String& field) {
    return "guide.vertex." + juce::String(vertexIndex) + "." + field;
}

juce::String TrimeshGuideAttachmentTarget::portIdForCube(
        int cubeIndex,
        const juce::String& field) {
    return "guide.cube." + juce::String(cubeIndex) + "." + field;
}

}

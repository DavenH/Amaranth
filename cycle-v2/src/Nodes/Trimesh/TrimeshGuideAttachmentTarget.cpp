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
    return vertexIndex >= 0 && fieldIndex() >= 0;
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
    if (!portId.startsWith("guide.vertex.")) {
        return {};
    }

    const juce::String suffix = portId.fromFirstOccurrenceOf("guide.vertex.", false, false);
    const juce::String vertexIndexText = suffix.upToFirstOccurrenceOf(".", false, false);
    const juce::String fieldText = suffix.fromFirstOccurrenceOf(".", false, false);

    if (vertexIndexText.isEmpty() || !vertexIndexText.containsOnly("0123456789")) {
        return {};
    }

    TrimeshGuideAttachmentTarget target {
            vertexIndexText.getIntValue(),
            fieldText
    };

    return target.isValid() ? target : TrimeshGuideAttachmentTarget {};
}

juce::String TrimeshGuideAttachmentTarget::portIdFor(
        int vertexIndex,
        const juce::String& field) {
    return "guide.vertex." + juce::String(vertexIndex) + "." + field;
}

}

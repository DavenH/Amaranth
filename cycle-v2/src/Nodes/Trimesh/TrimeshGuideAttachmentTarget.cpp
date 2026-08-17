#include "TrimeshGuideAttachmentTarget.h"

#include "../../Graph/NodeGraph.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>

#include <algorithm>

#include "TrimeshMeshState.h"

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

GuideCurveField TrimeshGuideAttachmentTarget::guideField(const juce::String& field) {
    if (field == "time") {
        return GuideCurveField::Time;
    }
    if (field == "red") {
        return GuideCurveField::Red;
    }
    if (field == "blue") {
        return GuideCurveField::Blue;
    }
    if (field == "phase") {
        return GuideCurveField::Phase;
    }
    if (field == "amp") {
        return GuideCurveField::Amplitude;
    }
    return GuideCurveField::Curve;
}

bool TrimeshGuideAttachmentTarget::isValid() const {
    return cubeIndex >= 0 && fieldIndex() >= 0;
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
    const juce::String prefix = "guide.cube.";
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
            targetIndexText.getIntValue(),
            fieldText
    };

    return target.isValid() ? target : TrimeshGuideAttachmentTarget {};
}

juce::String TrimeshGuideAttachmentTarget::portIdForCube(
        int cubeIndex,
        const juce::String& field) {
    return "guide.cube." + juce::String(cubeIndex) + "." + field;
}

std::vector<juce::String> TrimeshGuideAttachmentTarget::cubePortIdsForVertex(
        const Node& trimeshNode,
        int vertexIndex,
        const juce::String& field) {
    const auto model = std::dynamic_pointer_cast<const TrimeshNodeModelState>(trimeshNode.model);
    if (model == nullptr || !isPositiveAndBelow(vertexIndex, model->mesh().getNumVerts())) {
        return {};
    }

    Mesh& mesh = *const_cast<Mesh*>(&model->mesh());
    Vertex* vertex = mesh.getVerts()[(size_t) vertexIndex];
    std::vector<juce::String> targets;
    for (auto* owner : vertex->owners) {
        const auto found = std::find(mesh.getCubes().begin(), mesh.getCubes().end(), owner);
        if (found != mesh.getCubes().end()) {
            targets.push_back(portIdForCube(
                    (int) std::distance(mesh.getCubes().begin(), found),
                    field));
        }
    }
    return targets;
}

}

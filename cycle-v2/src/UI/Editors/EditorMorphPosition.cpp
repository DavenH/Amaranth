#include "UI/Editors/EditorMorphPosition.h"

#include <Curve/Mesh/MorphDimensions.h>

#include <utility>

namespace CycleV2 {

void EditorMorphPosition::reset() {
    preview.reset();
    lastAuthored.reset();
    values = {};
    for (auto& generation : previewGenerations) {
        ++generation;
    }
}

void EditorMorphPosition::setPreview(std::optional<MorphPosition> position) {
    for (size_t index = 0; index < MorphDimensions::axes.size(); ++index) {
        const int axis = MorphDimensions::axes[index];
        if (preview.has_value() != position.has_value()
                || (position.has_value() && (*position)[axis].getTargetValue()
                        != (*preview)[axis].getTargetValue())) {
            values[index].reset();
            ++previewGenerations[index];
        }
    }
    preview = std::move(position);
}

void EditorMorphPosition::setValue(int axis, float value) {
    for (size_t index = 0; index < MorphDimensions::axes.size(); ++index) {
        if (MorphDimensions::axes[index] == axis) {
            values[index] = juce::jlimit(0.f, 1.f, value);
            return;
        }
    }
}

void EditorMorphPosition::restoreValues(const Checkpoint& checkpoint) {
    for (size_t index = 0; index < values.size(); ++index) {
        values[index] = previewGenerations[index] == checkpoint.previewGenerations[index]
                ? checkpoint.values[index] : std::nullopt;
    }
}

MorphPosition EditorMorphPosition::resolve(const MorphPosition& authored, bool preserveValues) {
    MorphPosition result = preview.value_or(authored);
    for (size_t index = 0; index < MorphDimensions::axes.size(); ++index) {
        const int axis = MorphDimensions::axes[index];
        if (!preserveValues && lastAuthored.has_value()
                && authored[axis].getTargetValue() != (*lastAuthored)[axis].getTargetValue()) {
            values[index].reset();
        }
        if (values[index].has_value()) {
            result[axis].setValueDirect(*values[index]);
        }
    }
    lastAuthored = authored;
    return result;
}

}

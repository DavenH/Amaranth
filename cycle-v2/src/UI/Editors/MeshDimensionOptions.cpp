#include "UI/Editors/MeshDimensionOptions.h"

namespace CycleV2 {

namespace {

const juce::String names[] { "Yellow", "Red", "Blue" };
const juce::Colour colours[] { juce::Colour(0xffe0c247), juce::Colour(0xffd65a5a), juce::Colour(0xff5f91e8) };

juce::String axisNames(int mask) {
    juce::StringArray result;
    for (size_t axis = 0; axis < MorphDimensions::axes.size(); ++axis) {
        if ((mask & MorphDimensions::poleBits[axis]) != 0) {
            result.add(names[axis]);
        }
    }
    return result.joinIntoString(", ");
}

}

std::vector<PropertySegmentOption> meshDimensionOptions(MorphDimensions::Plane plane) {
    std::vector<PropertySegmentOption> options {
            { "Static", "0", "meshDimensions.static", "One curve without morph axes" },
            { "1D", "1", "meshDimensions.one", {} },
            { "2D", "2", "meshDimensions.two", {} },
            { "3D", "3", "meshDimensions.three", {} }
    };
    for (size_t count = 0; count < options.size(); ++count) {
        const MorphDimensions dimensions((int) count, plane);
        auto& option = options[count];
        if (count > 0) {
            option.tooltip = "Morph along " + axisNames(dimensions.cornerMask());
        }
        for (size_t axis = 0; axis < MorphDimensions::axes.size(); ++axis) {
            if (dimensions.active(MorphDimensions::axes[axis])) {
                option.ticks.push_back(colours[axis]);
            }
        }
    }
    return options;
}

juce::String meshDimensionTooltip(int current, int target, MorphDimensions::Plane plane) {
    const MorphDimensions before(current, plane);
    const MorphDimensions after(target, plane);
    const int dismissed = before.cornerMask() & ~after.cornerMask();
    const juce::String description = target == 0 ? "One curve without morph axes"
            : "Morph along " + axisNames(after.cornerMask());
    return dismissed == 0 ? description : description + "\n\nThis bakes the current "
            + axisNames(dismissed) + " position and discards variation on those axes. Undo restores it.";
}

}

#pragma once

#include <optional>

#include "ScalarSurfaceMaterial.h"

namespace TimeSurfaceStyles {

enum class Group { Canonical, Experimental, Legacy };

struct Entry {
    ScalarSurfaceTimeStyle style;
    const char* id;
    const char* label;
    Group group;
};

inline constexpr std::array<Entry, 14> entries {{
    { ScalarSurfaceTimeStyle::Greyscale, "greyscale", "Greyscale", Group::Canonical },
    { ScalarSurfaceTimeStyle::IcyHot13, "icy-hot", "Icy Hot", Group::Canonical },
    { ScalarSurfaceTimeStyle::BlueDepth, "blues", "Blues", Group::Canonical },
    { ScalarSurfaceTimeStyle::Bullion, "bullion", "Bullion", Group::Canonical },
    { ScalarSurfaceTimeStyle::Recipe19, "recipe-19", "Recipe 19", Group::Experimental },
    { ScalarSurfaceTimeStyle::Recipe18, "recipe-18", "Recipe 18", Group::Experimental },
    { ScalarSurfaceTimeStyle::Recipe17, "recipe-17", "Recipe 17", Group::Experimental },
    { ScalarSurfaceTimeStyle::Recipe16, "recipe-16", "Recipe 16", Group::Experimental },
    { ScalarSurfaceTimeStyle::Recipe15, "recipe-15", "Recipe 15", Group::Experimental },
    { ScalarSurfaceTimeStyle::IcyHot14, "recipe-14", "Icy-hot 14", Group::Legacy },
    { ScalarSurfaceTimeStyle::Bipolar, "bipolar-detail", "Bipolar + Detail", Group::Legacy },
    { ScalarSurfaceTimeStyle::BipolarFlat, "bipolar", "Bipolar (Colour Only)", Group::Legacy },
    { ScalarSurfaceTimeStyle::BipolarShaded, "bipolar-shaded", "Bipolar Shaded", Group::Legacy },
    { ScalarSurfaceTimeStyle::BlueDepthDirectionalDetail, "blue-detail", "Blue Depth + Directional Detail", Group::Legacy }
}};

const Entry* find(int index);
const Entry* find(const juce::String& id);
const char* id(ScalarSurfaceTimeStyle style);
ScalarSurfaceTimeStyle fromId(const juce::String& id);
juce::PopupMenu menu(
        std::optional<ScalarSurfaceTimeStyle> selected,
        int firstItemId,
        int defaultItemId = 0,
        const char* defaultLabel = nullptr);

}

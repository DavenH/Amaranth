#pragma once

#include <JuceHeader.h>
#include <array>

namespace CycleV2::WaveformColourStyles {

enum class Style { BurntAlum, IcyHot, IcyHotRaw, Bullion, Blues, Magenta, Greyscale };
inline constexpr auto defaultStyle = Style::BurntAlum;
// Dark backing preserves the translucent fill brightness while subduing the grid below it.
inline constexpr int fillGridBacking = 2;

struct Entry {
    Style style;
    const char* id;
    const char* label;
};

inline constexpr std::array<Entry, 7> entries {{
    { Style::BurntAlum, "burnt-alum", "Burnt Alum" },
    { Style::IcyHot, "icy-hot", "Icy Hot" },
    { Style::IcyHotRaw, "icy-hot-raw", "Icy Hot (Raw)" },
    { Style::Bullion, "bullion", "Bullion" },
    { Style::Blues, "blues", "Blues" },
    { Style::Magenta, "magenta", "Magenta" },
    { Style::Greyscale, "greyscale", "Greyscale" }
}};

const Entry* find(const juce::String& id);
Style fromId(const juce::String& id);
const char* id(Style style);
juce::Image gradient(Style style);
Style current();
void setCurrent(Style style);

}

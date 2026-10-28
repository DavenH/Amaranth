#include "UI/WaveformColourStyles.h"

#include <Binary/Gradients.h>
#include <UI/Panels/ScalarSurfaceMaterial.h>
#include <atomic>

namespace CycleV2::WaveformColourStyles {

namespace {
std::atomic<Style> selected { defaultStyle };
}

const Entry* find(const juce::String& styleId) {
    for (const auto& entry : entries) {
        if (styleId == entry.id) {
            return &entry;
        }
    }
    return nullptr;
}

Style fromId(const juce::String& styleId) {
    const auto* entry = find(styleId);
    return entry != nullptr ? entry->style : defaultStyle;
}

const char* id(Style style) {
    for (const auto& entry : entries) {
        if (style == entry.style) {
            return entry.id;
        }
    }
    return id(defaultStyle);
}

juce::Image gradient(Style style) {
    switch (style) {
        case Style::BurntAlum:
            return juce::PNGImageFormat::loadFrom(Gradients::burntalum_png, Gradients::burntalum_pngSize);
        case Style::Magenta:
            return juce::PNGImageFormat::loadFrom(Gradients::magenta_png, Gradients::magenta_pngSize);
        case Style::IcyHot:
            return ScalarSurfaceMaterialEvaluator::createGradientImage(ScalarSurfaceMaterial::icyHotProgram(false));
        case Style::IcyHotRaw:
            return ScalarSurfaceMaterialEvaluator::createGradientImage(
                    ScalarSurfaceMaterial::forTimeStyle(ScalarSurfaceTimeStyle::IcyHotRaw));
        case Style::Bullion:
            return ScalarSurfaceMaterialEvaluator::createGradientImage(
                    ScalarSurfaceMaterial::forTimeStyle(ScalarSurfaceTimeStyle::Bullion));
        case Style::Blues:
            return ScalarSurfaceMaterialEvaluator::createGradientImage(ScalarSurfaceMaterial::blueDepth());
        case Style::Greyscale:
            return ScalarSurfaceMaterialEvaluator::createGradientImage(ScalarSurfaceMaterial::greyscale());
    }
    jassertfalse;
    return {};
}

Style current() { return selected.load(); }
void setCurrent(Style style) { selected.store(style); }

}

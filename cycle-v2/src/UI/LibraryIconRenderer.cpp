#include "UI/LibraryIconRenderer.h"

#include <array>

#include "UI/CanvasChromePalette.h"
#include "UiIconData.h"

namespace CycleV2 {

namespace {

constexpr std::array<const char*, (size_t) LibraryIcon::Count> names {
        "libraryCurves", "libraryPresets", "libraryPatterns", "libraryNodes", "libraryAssets",
        "libraryImport", "libraryAddReference", "libraryTray"
};

struct Icon {
    std::unique_ptr<juce::Drawable> resting;
    std::unique_ptr<juce::Drawable> highlighted;
};

const Icon* iconFor(LibraryIcon icon) {
    static const auto icons = [] {
        std::array<Icon, names.size()> result;
        for (size_t index = 0; index < names.size(); ++index) {
            for (const auto& source : UiIconData::sources) {
                if (juce::String(source.name) != names[index]) {
                    continue;
                }
                if (const auto xml = juce::parseXML(source.svg)) {
                    auto& target = result[index];
                    target.resting = juce::Drawable::createFromSVG(*xml);
                    if (target.resting != nullptr) {
                        target.highlighted = target.resting->createCopy();
                        target.highlighted->replaceColour(CanvasChromePalette::mutedText,
                                CanvasChromePalette::text);
                    }
                }
            }
        }
        return result;
    }();
    const auto index = (size_t) icon;
    return index < icons.size() ? &icons[index] : nullptr;
}

}

void LibraryIconRenderer::configure(juce::Button& button, LibraryIcon icon,
        const juce::String& tooltip) {
    button.getProperties().set("libraryIcon", (int) icon);
    button.setTooltip(tooltip);
    button.setTitle(tooltip);
    button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

bool LibraryIconRenderer::hasIcon(LibraryIcon icon) {
    const auto* item = iconFor(icon);
    return item != nullptr && item->resting != nullptr && item->highlighted != nullptr;
}

void LibraryIconRenderer::paint(juce::Graphics& graphics, LibraryIcon icon,
        juce::Rectangle<float> canvas, bool highlighted) {
    const auto* item = iconFor(icon);
    if (item == nullptr) {
        return;
    }
    const auto* drawable = highlighted ? item->highlighted.get() : item->resting.get();
    if (drawable != nullptr) {
        drawable->draw(graphics, 1.f, juce::RectanglePlacement(juce::RectanglePlacement::centred)
                .getTransformToFit({ 0.f, 0.f, 24.f, 24.f }, canvas));
    }
}

bool LibraryIconRenderer::paintButton(juce::Graphics& graphics, juce::Button& button) {
    const auto value = button.getProperties().getWithDefault("libraryIcon", -1);
    if ((int) value < 0) {
        return false;
    }
    auto canvas = button.getLocalBounds().toFloat().withSizeKeepingCentre(24.f, 24.f);
    if ((bool) button.getProperties().getWithDefault("leadingIcon", false)) {
        canvas.setX(8.f);
    }
    paint(graphics, (LibraryIcon) (int) value, canvas,
            button.getToggleState() || button.isMouseOver() || button.hasKeyboardFocus(false));
    return true;
}

}

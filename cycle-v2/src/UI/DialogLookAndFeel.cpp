#include "UI/DialogLookAndFeel.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {
using namespace juce;

DialogLookAndFeel::DialogLookAndFeel() : AmaranthLookAndFeel(nullptr, false) {
    setColour(AlertWindow::backgroundColourId, Colour(0xff333333));
    setColour(AlertWindow::textColourId, CanvasChromePalette::text);
}

Font DialogLookAndFeel::getAlertWindowTitleFont() {
    return Font(FontOptions(CanvasChromeMetrics::editorTitleFontSize).withStyle("Bold"));
}

Font DialogLookAndFeel::getAlertWindowMessageFont() {
    return Font(FontOptions(13.f));
}

Font DialogLookAndFeel::getAlertWindowFont() {
    return getAlertWindowMessageFont();
}

void DialogLookAndFeel::drawAlertBox(Graphics& graphics, AlertWindow& alert,
        const Rectangle<int>& textArea, TextLayout& layout) {
    graphics.fillAll(alert.findColour(AlertWindow::backgroundColourId));
    graphics.setColour(alert.findColour(AlertWindow::outlineColourId));
    graphics.drawRect(alert.getLocalBounds(), 1);
    layout.draw(graphics, textArea.toFloat());
}

StyledAlertWindow::StyledAlertWindow(const String& title, const String& message,
        Component* associated) : AlertWindow(title, message, MessageBoxIconType::NoIcon, associated) {
    setLookAndFeel(&theme);
}

StyledAlertWindow::~StyledAlertWindow() {
    setLookAndFeel(nullptr);
}

}

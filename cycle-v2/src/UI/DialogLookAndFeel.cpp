#include "UI/DialogLookAndFeel.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {
using namespace juce;

DialogLookAndFeel::DialogLookAndFeel() {
    setColour(AlertWindow::backgroundColourId, CanvasChromePalette::dockSurface);
    setColour(AlertWindow::textColourId, CanvasChromePalette::text);
    setColour(AlertWindow::outlineColourId, CanvasChromePalette::border);
    setColour(TextButton::buttonColourId, CanvasChromePalette::raisedSurface);
    setColour(TextButton::buttonOnColourId, CanvasChromePalette::border);
    setColour(TextButton::textColourOffId, CanvasChromePalette::text);
    setColour(TextButton::textColourOnId, CanvasChromePalette::text);
    setColour(TextEditor::backgroundColourId, CanvasChromePalette::restingControlSurface);
    setColour(TextEditor::textColourId, CanvasChromePalette::text);
    setColour(TextEditor::outlineColourId, CanvasChromePalette::border);
    setColour(TextEditor::focusedOutlineColourId, CanvasChromePalette::focus);
    setColour(TextEditor::highlightColourId, CanvasChromePalette::areaSelection);
    setColour(TextEditor::highlightedTextColourId, CanvasChromePalette::text);
    setColour(Label::textColourId, CanvasChromePalette::text);
    setColour(ComboBox::backgroundColourId, CanvasChromePalette::restingControlSurface);
    setColour(ComboBox::textColourId, CanvasChromePalette::text);
    setColour(ComboBox::outlineColourId, CanvasChromePalette::border);
    setColour(ComboBox::arrowColourId, CanvasChromePalette::mutedText);
    setColour(Slider::textBoxBackgroundColourId, CanvasChromePalette::restingControlSurface);
    setColour(Slider::textBoxTextColourId, CanvasChromePalette::text);
    setColour(Slider::textBoxOutlineColourId, CanvasChromePalette::border);
    setColour(ProgressBar::backgroundColourId, CanvasChromePalette::restingControlSurface);
    setColour(ProgressBar::foregroundColourId, CanvasChromePalette::areaSelection);
}

Font DialogLookAndFeel::getTextButtonFont(TextButton&, int) {
    return Font(FontOptions(13.f).withStyle("Bold"));
}

void DialogLookAndFeel::drawButtonBackground(Graphics& graphics, Button& button,
        const Colour& colour, bool over, bool down) {
    PresetBrowserLookAndFeel::drawButtonBackground(graphics, button, colour, over, down);
    if (button.isEnabled() && button.hasKeyboardFocus(false)) {
        graphics.setColour(CanvasChromePalette::focus);
        graphics.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(1.f),
                CanvasChromeMetrics::controlCornerRadius, CanvasChromeMetrics::focusRingWidth);
    }
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

int DialogLookAndFeel::getAlertWindowButtonHeight() {
    return 32;
}

void DialogLookAndFeel::drawAlertBox(Graphics& graphics, AlertWindow& alert,
        const Rectangle<int>& textArea, TextLayout& layout) {
    graphics.fillAll(alert.findColour(AlertWindow::backgroundColourId));
    graphics.setColour(alert.findColour(AlertWindow::outlineColourId));
    graphics.drawRect(alert.getLocalBounds(), 1);
    layout.draw(graphics, textArea.toFloat());
}

void DialogLookAndFeel::fillResizableWindowBackground(
        Graphics& graphics, int, int, const BorderSize<int>&, ResizableWindow& window) {
    graphics.fillAll(window.findColour(ResizableWindow::backgroundColourId));
}

void DialogLookAndFeel::drawProgressBar(Graphics& graphics, ProgressBar& bar,
        int width, int height, double value, const String& text) {
    graphics.fillAll(bar.findColour(ProgressBar::backgroundColourId));
    graphics.setColour(bar.findColour(ProgressBar::foregroundColourId));
    if (value >= 0.0 && value <= 1.0) {
        graphics.fillRect(0, 0, roundToInt(width * value), height);
    } else {
        const int segment = jmax(1, width / 5);
        const int position = (int) ((Time::getMillisecondCounter() / 16) % (width + segment)) - segment;
        graphics.fillRect(position, 0, segment, height);
    }
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(FontOptions(12.f));
    graphics.drawText(text, 0, 0, width, height, Justification::centred);
}

void DialogLookAndFeel::markPrimary(Button& button) {
    button.setColour(TextButton::buttonColourId, CanvasChromePalette::areaSelection);
    button.setColour(TextButton::buttonOnColourId, CanvasChromePalette::focus);
}

StyledAlertWindow::StyledAlertWindow(const String& title, const String& message,
        Component* associated) : AlertWindow(title, message, MessageBoxIconType::NoIcon, associated) {
    setLookAndFeel(&theme);
}

StyledAlertWindow::~StyledAlertWindow() {
    setLookAndFeel(nullptr);
}

}

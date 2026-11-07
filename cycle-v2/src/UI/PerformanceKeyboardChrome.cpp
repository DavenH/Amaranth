#include "UI/PerformanceKeyboard.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"
#include "UI/WorkspaceDock.h"

namespace CycleV2 {

PerformanceKeyboardPanel::PatternButton::PatternButton(Action buttonAction) :
        action(buttonAction) {
    setTitle(action == Action::Play ? "Play MIDI pattern"
            : action == Action::Record ? "Record MIDI pattern" : "Edit MIDI pattern");
    setMouseClickGrabsKeyboardFocus(false);
    setMouseCursor(MouseCursor::PointingHandCursor);
}

void PerformanceKeyboardPanel::PatternButton::paintButton(
        Graphics& graphics, bool hovered, bool down) {
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const bool active = getToggleState();
    paintPanelOverlayButton(graphics, bounds, hovered, down,
            hasKeyboardFocus(false), active);
    graphics.setColour(active && action == Action::Record
            ? Colour(0xffef6a6a) : CanvasChromePalette::text);
    const auto icon = bounds.withSizeKeepingCentre(14.f, 14.f);
    if (active && action != Action::Edit) {
        graphics.fillRoundedRectangle(icon.reduced(2.f), 1.f);
    } else if (action == Action::Play) {
        Path triangle;
        triangle.addTriangle(icon.getX() + 2.f, icon.getY(),
                icon.getRight(), icon.getCentreY(), icon.getX() + 2.f, icon.getBottom());
        graphics.fillPath(triangle);
    } else if (action == Action::Record) {
        graphics.drawEllipse(icon.reduced(1.f), 1.3f);
    } else {
        graphics.drawVerticalLine(roundToInt(icon.getX()), icon.getY(), icon.getBottom());
        for (int row = 0; row < 3; ++row) {
            const float y = icon.getY() + 1.f + row * 5.f;
            const float x = icon.getX() + 4.f + (row % 2) * 3.f;
            graphics.fillRoundedRectangle(x, y, row == 1 ? 6.f : 4.f, 2.f, 0.5f);
        }
    }
}

void PerformanceKeyboardPanel::paint(Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::dockSurface);
    graphics.setColour(CanvasChromePalette::border);
    graphics.drawHorizontalLine(getHeight() - 1, 0.f, (float) getWidth());
    const Rectangle<float> actions = playBounds().getUnion(editBounds());
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(10.f));
    graphics.drawText("MIDI pattern", actions.withY(3.f).withHeight(12.f), Justification::centred);
    const auto track = progressBounds();
    graphics.setColour(CanvasChromePalette::strongBorder.withAlpha(0.22f));
    graphics.fillRect(track);
    if (progress > 0.f) {
        graphics.setColour(CanvasChromePalette::focus.withAlpha(0.7f));
        graphics.fillRect(track.withWidth(track.getWidth() * progress));
    }
}

void PerformanceKeyboardPanel::resized() {
    auto content = getLocalBounds().reduced(18, 6);
    auto actions = content.removeFromLeft(104);
    const int actionY = jmax(16, (getHeight() - 28) / 2 + 5);
    playButton.setBounds(actions.getX(), actionY, 28, 28);
    recordButton.setBounds(actions.getX() + 38, actionY, 28, 28);
    editButton.setBounds(actions.getX() + 76, actionY, 28, 28);
    content.removeFromLeft(16);
    const int dialCount = 1 + (int) extraWheels.size();
    const int dialSpace = jmin(dialCount * 44, jmax(44, content.getWidth() - 282));
    const int keysWidth = jmin(360, jmax(0, content.getWidth() - dialSpace - 72));
    octaveDown.setBounds(content.removeFromLeft(28));
    content.removeFromLeft(2);
    keyboard.setBounds(content.removeFromLeft(keysWidth));
    content.removeFromLeft(2);
    octaveUp.setBounds(content.removeFromLeft(28));
    content.removeFromLeft(12);
    controllerViewport.setBounds(content.removeFromLeft(dialSpace).withY(2).withHeight(getHeight() - 4));
    controllerStrip.setSize(dialCount * 44, controllerViewport.getHeight() - 3);
    modWheel.setBounds(0, 0, 44, controllerStrip.getHeight());
    for (size_t index = 0; index < extraWheels.size(); ++index) {
        extraWheels[index]->setBounds((int) (index + 1) * 44, 0, 44, controllerStrip.getHeight());
    }
}

PerformanceKeyboardPanel::ModWheel::ModWheel(int controller) :
        controllerNumber(controller) {
    setName(controller == 1 ? "PerformanceKeyboard.ModWheel"
            : "PerformanceKeyboard.CC" + String(controller));
    setSliderStyle(Slider::RotaryVerticalDrag);
    setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    setRange(0, 127, 1);
    setMouseDragSensitivity(254);
    setVelocityBasedMode(false);
    setVelocityModeParameters(0.1, 1, 0.0, false);
    setWantsKeyboardFocus(true);
    setMouseCursor(MouseCursor::UpDownResizeCursor);
}

void PerformanceKeyboardPanel::ModWheel::mouseDown(const MouseEvent& event) {
    setMouseDragSensitivity(event.mods.isShiftDown() ? 2540 : 254);
    Slider::mouseDown(event);
}

void PerformanceKeyboardPanel::ModWheel::setValue(int nextValue, bool notify) {
    Slider::setValue(nextValue, notify ? sendNotificationSync : dontSendNotification);
}

void PerformanceKeyboardPanel::ModWheel::valueChanged() {
    repaint();
    if (onValueChanged) {
        onValueChanged(value());
    }
}

void PerformanceKeyboardPanel::ModWheel::startedDragging() {
    if (onGestureStarted) {
        onGestureStarted();
    }
}

void PerformanceKeyboardPanel::ModWheel::stoppedDragging() {
    if (onGestureEnded) {
        onGestureEnded();
    }
}

bool PerformanceKeyboardPanel::ModWheel::keyPressed(const KeyPress& key) {
    const int code = key.getKeyCode();
    if (code == KeyPress::homeKey || code == KeyPress::endKey) {
        setValue(code == KeyPress::homeKey ? 0 : 127, true);
        return true;
    }
    if (code == KeyPress::upKey || code == KeyPress::rightKey
            || code == KeyPress::downKey || code == KeyPress::leftKey) {
        setValue(value() + (code == KeyPress::upKey || code == KeyPress::rightKey ? 1 : -1), true);
        return true;
    }
    return false;
}

String PerformanceKeyboardPanel::ModWheel::getTooltip() {
    return (controllerNumber == 1 ? String("MOD") : "MIDI CC " + String(controllerNumber))
            + ": " + String(value()) + " / 127. Drag to adjust; Shift for fine control.";
}

void PerformanceKeyboardPanel::ModWheel::paint(Graphics& graphics) {
    constexpr float start = MathConstants<float>::pi * 1.25f;
    constexpr float end = MathConstants<float>::pi * 2.75f;
    const float angle = start + (end - start) * (float) getValue() / 127.f;
    const Rectangle<float> dial((getWidth() - 26.f) * 0.5f, 2.f, 26.f, 26.f);
    const auto centre = dial.getCentre();
    Path track;
    track.addCentredArc(centre.x, centre.y, 12.f, 12.f, 0.f, start, end, true);
    graphics.setColour(CanvasChromePalette::strongBorder);
    graphics.strokePath(track, PathStrokeType(2.4f, PathStrokeType::curved, PathStrokeType::rounded));
    Path fill;
    fill.addCentredArc(centre.x, centre.y, 12.f, 12.f, 0.f, start, angle, true);
    graphics.setColour(Colour(0xff34d3df));
    graphics.strokePath(fill, PathStrokeType(2.4f, PathStrokeType::curved, PathStrokeType::rounded));
    graphics.setColour(CanvasChromePalette::text);
    graphics.drawLine(Line<float>(centre.getPointOnCircumference(5.f, angle),
            centre.getPointOnCircumference(9.f, angle)), 1.4f);
    graphics.setFont(FontOptions(9.f));
    graphics.drawText(controllerNumber == 1 ? "MOD" : "CC " + String(controllerNumber),
            Rectangle<float>(0.f, 29.f, (float) getWidth(), 10.f), Justification::centred);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.drawText(String(value()), Rectangle<float>(0.f, 39.f, (float) getWidth(), 10.f),
            Justification::centred);
    if (hasKeyboardFocus(false)) {
        graphics.setColour(CanvasChromePalette::focus);
        graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.f, 1.f);
    }
}

PerformanceKeyboardPanel::OctaveButton::OctaveButton(bool advancesOctave) :
        Button      (advancesOctave ? "Next keyboard octave" : "Previous keyboard octave")
    ,   advances    (advancesOctave) {
    setName(advances ? "PerformanceKeyboard.OctaveUp" : "PerformanceKeyboard.OctaveDown");
    setMouseCursor(MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true);
}

void PerformanceKeyboardPanel::OctaveButton::paintButton(
        Graphics& graphics,
        bool highlighted,
        bool down) {
    const Rectangle<float> bounds = getLocalBounds().toFloat().reduced(0.5f);
    WorkspaceDock::paintIconButton(
            graphics,
            bounds,
            advances ? WorkspaceDockIcon::ChevronRight : WorkspaceDockIcon::ChevronLeft,
            highlighted || hasKeyboardFocus(true));
    if (down) {
        graphics.setColour(Colours::white.withAlpha(0.08f));
        graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::controlCornerRadius);
    }
}


}

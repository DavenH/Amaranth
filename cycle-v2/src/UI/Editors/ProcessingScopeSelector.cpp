#include "UI/Editors/ProcessingScopeSelector.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/PropertyControls.h"

using namespace juce;

namespace CycleV2 {

namespace {

var boundsToAutomation(Rectangle<int> bounds) {
    auto* result = new DynamicObject();
    result->setProperty("x", bounds.getX());
    result->setProperty("y", bounds.getY());
    result->setProperty("width", bounds.getWidth());
    result->setProperty("height", bounds.getHeight());
    return var(result);
}

}

class ProcessingScopeSelector::ScopeButton final : public Button {
public:
    ScopeButton(String accessibleName, String text) :
            Button (std::move(accessibleName))
        ,   label  (std::move(text)) {
        setMouseCursor(MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }

    void paintButton(Graphics& graphics, bool highlighted, bool down) override {
        graphics.setColour(Colour(0xffe2e8ef).withAlpha(
                getToggleState() ? 1.f : highlighted || down ? 0.85f : 0.62f));
        graphics.setFont(FontOptions(10.f).withStyle(
                getToggleState() ? "Bold" : "Regular"));
        graphics.drawText(label, getLocalBounds(), Justification::centred);
        if (getToggleState()) {
            graphics.fillRect(getLocalBounds().removeFromBottom(2).reduced(8, 0));
        }
        if (hasKeyboardFocus(false)) {
            graphics.setColour(Colour(0xff65b8ff));
            graphics.drawRoundedRectangle(
                    getLocalBounds().toFloat().reduced(1.5f),
                    CanvasChromeMetrics::controlCornerRadius,
                    CanvasChromeMetrics::focusRingWidth);
        }
    }

    bool keyPressed(const KeyPress& key) override {
        if (key == KeyPress::leftKey) {
            navigate(-1);
            return true;
        }
        if (key == KeyPress::rightKey) {
            navigate(1);
            return true;
        }
        if (key == KeyPress::returnKey || key == KeyPress(' ')) {
            if (onClick) {
                onClick();
            }
            return true;
        }
        return Button::keyPressed(key);
    }

    std::function<void(int)> navigate;

private:
    String label;
};

ProcessingScopeSelector::ProcessingScopeSelector(String componentIdPrefix) :
        voice  (std::make_unique<ScopeButton>("Voice processing", "Voice"))
    ,   global (std::make_unique<ScopeButton>("Global processing", "Global")) {
    setComponentID(componentIdPrefix + ".processingScope");
    voice->setComponentID(componentIdPrefix + ".processingScope.voice");
    global->setComponentID(componentIdPrefix + ".processingScope.global");
    voice->setTooltip("Process separately inside each synth voice");
    global->setTooltip("Process once after active voices are mixed");
    voice->onClick = [this] { selectSegment(0, sendNotificationSync); };
    global->onClick = [this] { selectSegment(1, sendNotificationSync); };
    voice->navigate = [this](int offset) { focusSegment(offset > 0 ? 1 : 0); };
    global->navigate = [this](int offset) { focusSegment(offset < 0 ? 0 : 1); };
    addAndMakeVisible(*voice);
    addAndMakeVisible(*global);
    selectSegment(0, dontSendNotification);
}

ProcessingScopeSelector::~ProcessingScopeSelector() = default;

void ProcessingScopeSelector::setScope(
        const String& nextScope,
        NotificationType notification) {
    selectSegment(nextScope == "global" ? 1 : 0, notification);
}

String ProcessingScopeSelector::scope() const {
    return selectedSegment == 1 ? "global" : "voice";
}

var ProcessingScopeSelector::automationState() const {
    auto* state = new DynamicObject();
    state->setProperty("scope", scope());
    state->setProperty("selectedSegment", selectedSegment);
    state->setProperty("bounds", boundsToAutomation(getBounds()));
    state->setProperty("voiceBounds", boundsToAutomation(voice->getBounds()));
    state->setProperty("globalBounds", boundsToAutomation(global->getBounds()));
    return state;
}

void ProcessingScopeSelector::paint(Graphics& graphics) {
    paintPropertySegmentedControl(
            graphics,
            getLocalBounds().toFloat().reduced(0.75f),
            2,
            selectedSegment);
}

void ProcessingScopeSelector::resized() {
    const int split = getWidth() / 2;
    voice->setBounds(0, 0, split, getHeight());
    global->setBounds(split, 0, getWidth() - split, getHeight());
}

void ProcessingScopeSelector::focusSegment(int index) {
    (index == 0 ? voice.get() : global.get())->grabKeyboardFocus();
}

void ProcessingScopeSelector::selectSegment(
        int index,
        NotificationType notification) {
    const int next = jlimit(0, 1, index);
    const bool changed = selectedSegment != next;
    selectedSegment = next;
    voice->setToggleState(selectedSegment == 0, dontSendNotification);
    global->setToggleState(selectedSegment == 1, dontSendNotification);
    repaint();
    if (changed && notification != dontSendNotification && onChange) {
        onChange(scope());
    }
}

}

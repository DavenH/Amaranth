#include "UI/Editors/MorphKeyboardShortcutHelp.h"

#include "UI/Editors/MorphKeyboardShortcuts.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"

namespace CycleV2 {

using namespace juce;

class MorphKeyboardShortcutHelp::Target final :
        public Component
    ,   public SettableTooltipClient {
public:
    Target(const String& componentId, bool includesYellowAxis) {
        setComponentID(componentId);
        setName("MorphKeyboardShortcutTarget");
        setTitle("Morph keyboard shortcuts");
        const String help = morphKeyboardShortcutHelpText(includesYellowAxis);
        setDescription(help);
        setTooltip(help);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }
};

MorphKeyboardShortcutHelp::MorphKeyboardShortcutHelp(
        Component& host,
        const String& componentId,
        bool includesYellowAxis) :
        target(std::make_unique<Target>(componentId, includesYellowAxis)) {
    host.addChildComponent(target.get());
}

MorphKeyboardShortcutHelp::~MorphKeyboardShortcutHelp() = default;

void MorphKeyboardShortcutHelp::setAnchorBounds(Rectangle<float> focusOutlineBounds) {
    target->setBounds(
            keyboardFocusIndicatorHitBounds(focusOutlineBounds).toNearestInt());
    target->setVisible(true);
    target->toFront(false);
}

void MorphKeyboardShortcutHelp::hide() {
    target->setVisible(false);
}

}

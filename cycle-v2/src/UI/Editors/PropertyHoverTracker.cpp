#include <utility>

#include "UI/Editors/PropertyHoverTracker.h"

namespace CycleV2 {

PropertyHoverTracker::PropertyHoverTracker(juce::Component& component,
        std::function<void(juce::Point<float>)> callback) :
        owner  (component)
    ,   update (std::move(callback)) {
    startTimerHz(30);
}

void PropertyHoverTracker::timerCallback() {
    if (owner.isShowing() && juce::Process::isForegroundProcess()) {
        update(owner.getMouseXYRelative().toFloat());
    }
}

}

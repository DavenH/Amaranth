#include "UI/WorkspaceDockKeyboardNavigation.h"

#include <algorithm>

namespace CycleV2 {

std::vector<WorkspaceDockFocus> WorkspaceDockKeyboardNavigation::focusOrder(
        const WorkspaceDockKeyboardModel& model) {
    std::vector<WorkspaceDockFocus> order;
    if (model.guidesMinimized) {
        order.push_back({ WorkspaceDockFocusTarget::GuideDrawer, {} });
    } else {
        order.push_back({ WorkspaceDockFocusTarget::GuideMinimize, {} });
        order.push_back({ WorkspaceDockFocusTarget::GuideAdd, {} });
        for (const auto& guideId : model.guideIds) {
            order.push_back({ WorkspaceDockFocusTarget::GuideTile, guideId });
        }
    }

    return order;
}

bool WorkspaceDockKeyboardNavigation::moveFocus(
        const juce::KeyPress& key,
        const WorkspaceDockKeyboardModel& model,
        WorkspaceDockFocus& focus) {
    if (key.getKeyCode() == juce::KeyPress::tabKey) {
        const int direction = key.getModifiers().isShiftDown() ? -1 : 1;
        focus = WorkspaceDock::advanceFocus(focusOrder(model), focus, direction);
        return true;
    }

    if (focus.target == WorkspaceDockFocusTarget::GuideTile) {
        if (key.getKeyCode() == juce::KeyPress::upKey) {
            return moveWithinTiles(model.guideIds, -1, focus.target, focus);
        }
        if (key.getKeyCode() == juce::KeyPress::downKey) {
            return moveWithinTiles(model.guideIds, 1, focus.target, focus);
        }
    }
    return false;
}

bool WorkspaceDockKeyboardNavigation::keyPressed(
        const juce::KeyPress& key,
        const WorkspaceDockKeyboardModel& model,
        const WorkspaceDockKeyboardLayout& layout,
        WorkspaceDockFocus& focus,
        float& guideOffset,
        WorkspaceDockKeyboardDelegate& delegate) {
    if (moveFocus(key, model, focus)) {
        revealFocus(model, layout, focus, guideOffset);
        delegate.repaintDockFromKeyboard();
        return true;
    }
    if (focus.target == WorkspaceDockFocusTarget::None) {
        return false;
    }
    if (key.getKeyCode() == juce::KeyPress::returnKey
            || key.getKeyCode() == juce::KeyPress::spaceKey) {
        return activate(model, focus, delegate);
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        return remove(focus, delegate);
    }
    return false;
}

juce::String WorkspaceDockKeyboardNavigation::targetName(
        WorkspaceDockFocusTarget target) {
    switch (target) {
        case WorkspaceDockFocusTarget::None:            return "none";
        case WorkspaceDockFocusTarget::GuideDrawer:     return "guideDrawer";
        case WorkspaceDockFocusTarget::GuideMinimize:   return "guideMinimize";
        case WorkspaceDockFocusTarget::GuideAdd:        return "guideAdd";
        case WorkspaceDockFocusTarget::GuideTile:       return "guideTile";
    }
    return "none";
}

bool WorkspaceDockKeyboardNavigation::moveWithinTiles(
        const std::vector<juce::String>& ids,
        int direction,
        WorkspaceDockFocusTarget target,
        WorkspaceDockFocus& focus) {
    const auto found = std::find(ids.begin(), ids.end(), focus.itemId);
    if (found == ids.end()) {
        return false;
    }

    const int index = (int) std::distance(ids.begin(), found);
    const int next = juce::jlimit(0, (int) ids.size() - 1, index + direction);
    focus = { target, ids[(size_t) next] };
    return true;
}

void WorkspaceDockKeyboardNavigation::revealFocus(
        const WorkspaceDockKeyboardModel& model,
        const WorkspaceDockKeyboardLayout& layout,
        const WorkspaceDockFocus& focus,
        float& guideOffset) {
    if (focus.target != WorkspaceDockFocusTarget::GuideTile) {
        return;
    }
    const auto found = std::find(model.guideIds.begin(), model.guideIds.end(), focus.itemId);
    if (found != model.guideIds.end()) {
        const int index = (int) std::distance(model.guideIds.begin(), found);
        guideOffset = WorkspaceDock::offsetToRevealGuideTile(
                guideOffset, layout.maximumGuideOffset, layout.guideShelfHeight, index);
    }
}

bool WorkspaceDockKeyboardNavigation::activate(
        const WorkspaceDockKeyboardModel& model,
        WorkspaceDockFocus& focus,
        WorkspaceDockKeyboardDelegate& delegate) {
    switch (focus.target) {
        case WorkspaceDockFocusTarget::GuideDrawer:
        case WorkspaceDockFocusTarget::GuideMinimize:
            delegate.setGuideShelfMinimizedFromKeyboard(!model.guidesMinimized);
            break;
        case WorkspaceDockFocusTarget::GuideAdd: {
            const juce::String guideId = delegate.createGuideFromKeyboard();
            if (guideId.isNotEmpty()) {
                focus = { WorkspaceDockFocusTarget::GuideTile, guideId };
            }
            break;
        }
        case WorkspaceDockFocusTarget::GuideTile:
            delegate.selectGuideFromKeyboard(focus.itemId, true);
            break;
        case WorkspaceDockFocusTarget::None:
            return false;
    }
    delegate.repaintDockFromKeyboard();
    return true;
}

bool WorkspaceDockKeyboardNavigation::remove(
        WorkspaceDockFocus& focus,
        WorkspaceDockKeyboardDelegate& delegate) {
    if (focus.target == WorkspaceDockFocusTarget::GuideTile) {
        delegate.removeGuideFromKeyboard(focus.itemId);
    } else {
        return false;
    }

    focus = {};
    delegate.repaintDockFromKeyboard();
    return true;
}

}

#include "UI/WorkspaceDockKeyboardNavigation.h"

#include <algorithm>

namespace CycleV2 {

std::vector<WorkspaceDockFocus> WorkspaceDockKeyboardNavigation::focusOrder(
        const WorkspaceDockKeyboardModel& model) {
    std::vector<WorkspaceDockFocus> order;
    if (!model.expanded) {
        order.push_back({ WorkspaceDockFocusTarget::Collapse, {} });
        return order;
    }

    if (model.guidesMinimized) {
        order.push_back({ WorkspaceDockFocusTarget::GuideDrawer, {} });
    } else {
        order.push_back({ WorkspaceDockFocusTarget::GuideMinimize, {} });
        order.push_back({ WorkspaceDockFocusTarget::GuideAdd, {} });
        for (const auto& guideId : model.guideIds) {
            order.push_back({ WorkspaceDockFocusTarget::GuideTile, guideId });
        }
    }

    if (model.spiesMinimized) {
        if (!model.spyIds.empty()) {
            order.push_back({ WorkspaceDockFocusTarget::SpyDrawer, {} });
        }
    } else {
        if (!model.spyIds.empty()) {
            order.push_back({ WorkspaceDockFocusTarget::SpyMinimize, {} });
        }
        for (const auto& spyId : model.spyIds) {
            order.push_back({ WorkspaceDockFocusTarget::SpyTile, spyId });
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

    if (key.getKeyCode() == juce::KeyPress::leftKey) {
        if (focus.target == WorkspaceDockFocusTarget::SpyTile) {
            return moveWithinTiles(model.spyIds, -1, focus.target, focus);
        }
    }
    if (key.getKeyCode() == juce::KeyPress::rightKey) {
        if (focus.target == WorkspaceDockFocusTarget::SpyTile) {
            return moveWithinTiles(model.spyIds, 1, focus.target, focus);
        }
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
        float& spyOffset,
        WorkspaceDockKeyboardDelegate& delegate) {
    if (moveFocus(key, model, focus)) {
        revealFocus(model, layout, focus, guideOffset, spyOffset);
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
        case WorkspaceDockFocusTarget::Collapse:        return "collapse";
        case WorkspaceDockFocusTarget::GuideDrawer:     return "guideDrawer";
        case WorkspaceDockFocusTarget::GuideMinimize:   return "guideMinimize";
        case WorkspaceDockFocusTarget::GuideAdd:        return "guideAdd";
        case WorkspaceDockFocusTarget::GuideTile:       return "guideTile";
        case WorkspaceDockFocusTarget::SpyDrawer:       return "spyDrawer";
        case WorkspaceDockFocusTarget::SpyMinimize:     return "spyMinimize";
        case WorkspaceDockFocusTarget::SpyTile:         return "spyTile";
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
        float& guideOffset,
        float& spyOffset) {
    const bool guide = focus.target == WorkspaceDockFocusTarget::GuideTile;
    const bool spy = focus.target == WorkspaceDockFocusTarget::SpyTile;
    const auto& ids = guide ? model.guideIds : model.spyIds;
    const auto found = std::find(ids.begin(), ids.end(), focus.itemId);
    if ((!guide && !spy) || found == ids.end()) {
        return;
    }

    const int index = (int) std::distance(ids.begin(), found);
    float& offset = guide ? guideOffset : spyOffset;
    offset = guide
            ? WorkspaceDock::offsetToRevealGuideTile(
                    offset, layout.maximumGuideOffset, layout.guideShelfHeight, index)
            : WorkspaceDock::offsetToRevealTile(
                    offset, layout.maximumSpyOffset, layout.spyShelfWidth, index);
}

bool WorkspaceDockKeyboardNavigation::activate(
        const WorkspaceDockKeyboardModel& model,
        WorkspaceDockFocus& focus,
        WorkspaceDockKeyboardDelegate& delegate) {
    switch (focus.target) {
        case WorkspaceDockFocusTarget::Collapse:
            delegate.setDockExpandedFromKeyboard(!model.expanded);
            break;
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
        case WorkspaceDockFocusTarget::SpyDrawer:
        case WorkspaceDockFocusTarget::SpyMinimize:
            delegate.setSpyShelfMinimizedFromKeyboard(!model.spiesMinimized);
            break;
        case WorkspaceDockFocusTarget::SpyTile:
            delegate.selectSpyFromKeyboard(focus.itemId, true);
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
    if (focus.target != WorkspaceDockFocusTarget::SpyTile) {
        return false;
    }

    delegate.removeSpyFromKeyboard(focus.itemId);
    focus = {};
    delegate.repaintDockFromKeyboard();
    return true;
}

}

#include "WorkspaceDockKeyboardNavigation.h"

#include <algorithm>

namespace CycleV2 {

std::vector<WorkspaceDockFocus> WorkspaceDockKeyboardNavigation::focusOrder(
        const WorkspaceDockKeyboardModel& model) {
    std::vector<WorkspaceDockFocus> order { { WorkspaceDockFocusTarget::Collapse, {} } };
    if (!model.expanded) {
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
        order.push_back({ WorkspaceDockFocusTarget::SpyDrawer, {} });
    } else {
        order.push_back({ WorkspaceDockFocusTarget::SpyRefresh, {} });
        order.push_back({ WorkspaceDockFocusTarget::SpyMinimize, {} });
        for (const auto& spyId : model.spyIds) {
            order.push_back({ WorkspaceDockFocusTarget::SpyTile, spyId });
            order.push_back({ WorkspaceDockFocusTarget::SpyRemove, spyId });
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
        if (focus.target == WorkspaceDockFocusTarget::GuideTile) {
            return moveWithinTiles(model.guideIds, -1, focus.target, focus);
        }
        if (focus.target == WorkspaceDockFocusTarget::SpyTile) {
            return moveWithinTiles(model.spyIds, -1, focus.target, focus);
        }
    }
    if (key.getKeyCode() == juce::KeyPress::rightKey) {
        if (focus.target == WorkspaceDockFocusTarget::GuideTile) {
            return moveWithinTiles(model.guideIds, 1, focus.target, focus);
        }
        if (focus.target == WorkspaceDockFocusTarget::SpyTile) {
            return moveWithinTiles(model.spyIds, 1, focus.target, focus);
        }
    }
    if (key.getKeyCode() == juce::KeyPress::upKey
            || key.getKeyCode() == juce::KeyPress::downKey) {
        return moveBetweenShelves(model, focus);
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
        case WorkspaceDockFocusTarget::SpyRefresh:      return "spyRefresh";
        case WorkspaceDockFocusTarget::SpyTile:         return "spyTile";
        case WorkspaceDockFocusTarget::SpyRemove:       return "spyRemove";
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

bool WorkspaceDockKeyboardNavigation::moveBetweenShelves(
        const WorkspaceDockKeyboardModel& model,
        WorkspaceDockFocus& focus) {
    const std::vector<juce::String>* source = nullptr;
    const std::vector<juce::String>* destination = nullptr;
    WorkspaceDockFocusTarget destinationTarget { WorkspaceDockFocusTarget::None };
    if (focus.target == WorkspaceDockFocusTarget::GuideTile && !model.spyIds.empty()) {
        source = &model.guideIds;
        destination = &model.spyIds;
        destinationTarget = WorkspaceDockFocusTarget::SpyTile;
    } else if (focus.target == WorkspaceDockFocusTarget::SpyTile && !model.guideIds.empty()) {
        source = &model.spyIds;
        destination = &model.guideIds;
        destinationTarget = WorkspaceDockFocusTarget::GuideTile;
    } else {
        return false;
    }

    const auto found = std::find(source->begin(), source->end(), focus.itemId);
    const int index = found == source->end() ? 0 : (int) std::distance(source->begin(), found);
    const int destinationIndex = juce::jmin(index, (int) destination->size() - 1);
    focus = { destinationTarget, (*destination)[(size_t) destinationIndex] };
    return true;
}

void WorkspaceDockKeyboardNavigation::revealFocus(
        const WorkspaceDockKeyboardModel& model,
        const WorkspaceDockKeyboardLayout& layout,
        const WorkspaceDockFocus& focus,
        float& guideOffset,
        float& spyOffset) {
    const bool guide = focus.target == WorkspaceDockFocusTarget::GuideTile;
    const bool spy = focus.target == WorkspaceDockFocusTarget::SpyTile
            || focus.target == WorkspaceDockFocusTarget::SpyRemove;
    const auto& ids = guide ? model.guideIds : model.spyIds;
    const auto found = std::find(ids.begin(), ids.end(), focus.itemId);
    if ((!guide && !spy) || found == ids.end()) {
        return;
    }

    const int index = (int) std::distance(ids.begin(), found);
    float& offset = guide ? guideOffset : spyOffset;
    offset = WorkspaceDock::offsetToRevealTile(
            offset,
            guide ? layout.maximumGuideOffset : layout.maximumSpyOffset,
            guide ? layout.guideShelfWidth : layout.spyShelfWidth,
            index);
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
        case WorkspaceDockFocusTarget::SpyRefresh:
            delegate.toggleSpyRefreshFromKeyboard();
            break;
        case WorkspaceDockFocusTarget::SpyTile:
            delegate.selectSpyFromKeyboard(focus.itemId, true);
            break;
        case WorkspaceDockFocusTarget::SpyRemove:
            return remove(focus, delegate);
        case WorkspaceDockFocusTarget::None:
            return false;
    }
    delegate.repaintDockFromKeyboard();
    return true;
}

bool WorkspaceDockKeyboardNavigation::remove(
        WorkspaceDockFocus& focus,
        WorkspaceDockKeyboardDelegate& delegate) {
    if (focus.target != WorkspaceDockFocusTarget::SpyTile
            && focus.target != WorkspaceDockFocusTarget::SpyRemove) {
        return false;
    }

    delegate.removeSpyFromKeyboard(focus.itemId);
    focus = { WorkspaceDockFocusTarget::SpyRefresh, {} };
    delegate.repaintDockFromKeyboard();
    return true;
}

}

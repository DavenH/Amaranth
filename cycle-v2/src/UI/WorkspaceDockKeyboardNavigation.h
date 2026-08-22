#pragma once

#include <JuceHeader.h>

#include <vector>

#include "WorkspaceDock.h"

namespace CycleV2 {

struct WorkspaceDockKeyboardModel {
    bool expanded { true };
    bool guidesMinimized {};
    bool spiesMinimized {};
    std::vector<juce::String> guideIds;
    std::vector<juce::String> spyIds;
};

struct WorkspaceDockKeyboardLayout {
    float guideShelfWidth {};
    float spyShelfWidth {};
    float maximumGuideOffset {};
    float maximumSpyOffset {};
};

class WorkspaceDockKeyboardDelegate {
public:
    virtual ~WorkspaceDockKeyboardDelegate() = default;

    virtual void setDockExpandedFromKeyboard(bool expanded) = 0;
    virtual void setGuideShelfMinimizedFromKeyboard(bool minimized) = 0;
    virtual juce::String createGuideFromKeyboard() = 0;
    virtual void selectGuideFromKeyboard(const juce::String& guideId, bool openEditor) = 0;
    virtual void showGuideActionsFromKeyboard(const juce::String& guideId) = 0;
    virtual void setSpyShelfMinimizedFromKeyboard(bool minimized) = 0;
    virtual void toggleSpyRefreshFromKeyboard() = 0;
    virtual void selectSpyFromKeyboard(const juce::String& probeId, bool openDetail) = 0;
    virtual void removeSpyFromKeyboard(const juce::String& probeId) = 0;
    virtual void repaintDockFromKeyboard() = 0;
};

class WorkspaceDockKeyboardNavigation {
public:
    static std::vector<WorkspaceDockFocus> focusOrder(
            const WorkspaceDockKeyboardModel& model);
    static bool moveFocus(
            const juce::KeyPress& key,
            const WorkspaceDockKeyboardModel& model,
            WorkspaceDockFocus& focus);
    static bool keyPressed(
            const juce::KeyPress& key,
            const WorkspaceDockKeyboardModel& model,
            const WorkspaceDockKeyboardLayout& layout,
            WorkspaceDockFocus& focus,
            float& guideOffset,
            float& spyOffset,
            WorkspaceDockKeyboardDelegate& delegate);
    static juce::String targetName(WorkspaceDockFocusTarget target);

private:
    static bool moveWithinTiles(
            const std::vector<juce::String>& ids,
            int direction,
            WorkspaceDockFocusTarget target,
            WorkspaceDockFocus& focus);
    static bool moveBetweenShelves(
            const WorkspaceDockKeyboardModel& model,
            WorkspaceDockFocus& focus);
    static void revealFocus(
            const WorkspaceDockKeyboardModel& model,
            const WorkspaceDockKeyboardLayout& layout,
            const WorkspaceDockFocus& focus,
            float& guideOffset,
            float& spyOffset);
    static bool activate(
            const WorkspaceDockKeyboardModel& model,
            WorkspaceDockFocus& focus,
            WorkspaceDockKeyboardDelegate& delegate);
    static bool remove(
            WorkspaceDockFocus& focus,
            WorkspaceDockKeyboardDelegate& delegate);
};

}

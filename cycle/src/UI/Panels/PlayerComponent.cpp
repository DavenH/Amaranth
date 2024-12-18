#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <App/Doc/Document.h>
#include <App/EditWatcher.h>

#include "PlayerComponent.h"
#include "../Dialogs/PresetPage.h"
#include "../PluginWindow.h"
#include "../CycleGraphicsUtils.h"
#include "../Layout/ResizerPullout.h"
#include "../../App/Dialogs.h"
#include "../../Util/CycleEnums.h"

PlayerComponent::PlayerComponent(SingletonRepo* repo) :
        SingletonAccessor(repo, "PlayerComponent")
    ,	prevIcon(	2, 8, this, repo, "Previous Preset")
    ,	nextIcon(	3, 8, this, repo, "Next Preset")
    ,	tableIcon(	8, 5, this, repo, "Presets") {
    addAndMakeVisible(&prevIcon);
    addAndMakeVisible(&nextIcon);
    addAndMakeVisible(&tableIcon);

    prevIcon.addListener(this);
    nextIcon.addListener(this);
    tableIcon.addListener(this);

    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

void PlayerComponent::init() {
    keyboard = &getObj(MidiKeyboard);
    pullout = &getObj(ResizerPullout);
}

void PlayerComponent::setComponents(bool add) {
    if (add) {
        addAndMakeVisible(keyboard);
        addAndMakeVisible(pullout);
    } else {
        removeChildComponent(pullout);
        removeChildComponent(keyboard);
    }
}

void PlayerComponent::paint(Graphics& g) {
    getObj(CycleGraphicsUtils).fillBlackground(this, g);

    Font font(16);
    g.setFont(font);
    g.setColour(Colour::greyLevel(0.64f));
    g.setOpacity(1.f);

    Rectangle<int> textBounds(5, 0, getWidth() - 100, 28);

    String nameStr = name;
    if(getObj(EditWatcher).getHaveEdited()) {
        nameStr += "*";
    }

    Rectangle<int> r = textBounds.removeFromLeft(font.getStringWidth(nameStr));
    g.drawFittedText(nameStr, r.getX(), r.getY(), r.getWidth(), r.getHeight(), Justification::centredLeft, 1);

    textBounds.removeFromLeft(5);

    g.setOpacity(0.4f);
    String authStr = "by " + author;
    r = textBounds.removeFromLeft(font.getStringWidth(authStr));

    g.drawFittedText(authStr, r.getX(), r.getY(), r.getWidth(), r.getHeight(), Justification::centredLeft, 1);
}

void PlayerComponent::buttonClicked(Button* button) {
    if (button == &prevIcon) {
        getObj(PresetPage).triggerButtonClick(PresetPage::PrevButton);
    } else if (button == &nextIcon) {
        getObj(PresetPage).triggerButtonClick(PresetPage::NextButton);
    } else if (button == &tableIcon) {
        getObj(Dialogs).showPresetBrowserModal();
    }
}

void PlayerComponent::resized() {
    Rectangle<int> bounds = getBounds().withPosition(0, 0);
    bounds.removeFromTop(1);
    Rectangle<int> top = bounds.removeFromTop(28);

    top.removeFromRight(2);
    pullout->setBounds(top.removeFromRight(24));

    top.removeFromRight(6);
    tableIcon.setBounds(top.removeFromRight(24));

    top.removeFromRight(10);
    nextIcon.setBounds(top.removeFromRight(18));
    prevIcon.setBounds(top.removeFromRight(18));

    pullout->updateHighlight(getSetting(WindowSize));
    keyboard->setBounds(bounds.removeFromTop(82));
}

void PlayerComponent::updateTitle() {
     DocumentDetails& deets = getObj(Document).getDetails();

     name 	= deets.getName();
     author = deets.getAuthor();

     repaint();
}
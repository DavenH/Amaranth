#include <iostream>
#include "PulloutComponent.h"
#include "../MiscGraphics.h"
#include "../../App/SingletonRepo.h"
#include "../../Util/Util.h"

using std::cout;
using std::endl;

BoxComp::BoxComp() :
        delegator(this, this), horz(true), backgroundOpacity(0.5f) {
    setMouseCursor(MouseCursor::PointingHandCursor);
}

BoxComp::~BoxComp() {
}


void BoxComp::setPulloutComponent(PulloutComponent* pullout) {
    this->pullout = pullout;
}


void BoxComp::paint(Graphics& g) {
    g.fillAll(Colours::black.withAlpha(backgroundOpacity));

    if (horz) {
        g.setGradientFill(ColourGradient(Colours::darkgrey, getWidth(), 0, Colour::greyLevel(0.12f), 0, 0, false));
        g.drawVerticalLine(getWidth() - 1, 0, getHeight());
        g.drawHorizontalLine(getHeight() - 1, 0, getWidth());
        g.drawHorizontalLine(0, 0, getWidth());
    } else {
        g.setGradientFill(ColourGradient(Colours::darkgrey, 0, getHeight(), Colour::greyLevel(0.12f), 0, 0, false));
        g.drawHorizontalLine(getHeight() - 1, 0, getWidth());
        g.drawVerticalLine(getWidth() - 1, 0, getHeight());
        g.drawVerticalLine(0, 0, getHeight());
    }
}


void BoxComp::setHorizontal(bool isHorizontal) {
    horz = isHorizontal;
}


void BoxComp::mouseEnter(const MouseEvent& e) {
#ifndef JUCE_MAC
//	if(e.originalComponent == this)
    enterDlg();
#endif
}


void BoxComp::mouseExit(const MouseEvent& e) {
#ifndef JUCE_MAC
//	if(e.originalComponent == this)
    exitDlg();
#endif
}


void BoxComp::enterDlg() {
    setMouseCursor(MouseCursor::PointingHandCursor);
    stopTimer();
}


void BoxComp::exitDlg() {
    startTimer(menuDelayMillis);
}


void BoxComp::addAll(vector<Component*>& _buttons) {
    this->buttons = _buttons;
    int left = -24;
    int right = -24;

    for (vector<Component*>::iterator it = buttons.begin(); it != buttons.end(); ++it) {
        addAndMakeVisible(*it);
        (*it)->addMouseListener(this, true);
        (*it)->setBounds(horz ? left += 25 : 1, horz ? 0 : right += 25, 24, 24);
    }
}


void BoxComp::visibilityChanged() {
    int left = -24;
    int right = -24;

    if (isVisible()) {
        for (vector<Component*>::iterator it = buttons.begin(); it != buttons.end(); ++it) {
            if (!isParentOf(*it))
                addAndMakeVisible(*it);

            (*it)->setBounds(horz ? left += 25 : 1, horz ? 0 : right += 25, 24, 24);
        }
    } else {
        for (vector<Component*>::iterator it = buttons.begin(); it != buttons.end(); ++it) {
            if (isParentOf(*it))
                removeChildComponent(*it);
        }
    }
}


void BoxComp::timerCallback() {
    pullout->removeBoxFromDesktop();
    pullout->removedFromDesktop();
}


PulloutComponent::PulloutComponent(
        Image image, vector<Component*>& buttons, SingletonRepo* repo, bool horz) :
        SingletonAccessor(repo, "PulloutComponent"), img(image), horz(horz) {
    popup.setPulloutComponent(this);
//	img.duplicateIfShared();

    miscGraphics = &getObj(MiscGraphics);
    miscGraphics->addPulloutIcon(img, horz);

    popup.setHorizontal(horz);
    popup.addAll(buttons);
    popup.setAlwaysOnTop(true);

    horz ? popup.setSize(buttons.size() * 26, 26) : popup.setSize(26, buttons.size() * 26);
}


void PulloutComponent::paint(Graphics& g) {
    g.setOpacity(1.f);
    g.drawImageAt(img, 0, 0);

    Image pullout = miscGraphics->getIcon(7, horz ? 6 : 7);

    g.drawImageAt(pullout, 0, 0);
}


void PulloutComponent::mouseEnter(const MouseEvent& e) {
    int screenX = getScreenX();
    int screenY = getScreenY();

    popup.setTopLeftPosition(horz ? 26 + screenX : 2 + screenX, horz ? 2 + screenY : 26 + screenY);
    popup.addToDesktop(ComponentPeer::windowIsTemporary);
    popup.setVisible(true);
    popup.stopTimer();
}


void PulloutComponent::mouseExit(const MouseEvent& e) {
    popup.startTimer(menuDelayMillis);
}


Image& PulloutComponent::getImage() {
    return img;
}


void PulloutComponent::removedFromDesktop() {
}


void PulloutComponent::resized() {
    int screenX = getScreenX();
    int screenY = getScreenY();

    popup.setTopLeftPosition(horz ? 26 + screenX : 2 + screenX,
                             horz ? 2 + screenY : 26 + screenY);
}


void PulloutComponent::moved() {
}


void PulloutComponent::removeBoxFromDesktop() {
    popup.setVisible(false);
    popup.removeFromDesktop();
}


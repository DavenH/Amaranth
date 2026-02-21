#include <UI/MiscGraphics.h>
#include <Util/Arithmetic.h>
#include <UI/IConsole.h>
#include <App/SingletonAccessor.h>

#include "SelectorPanel.h"
#include <Definitions.h>
#include <App/SingletonRepo.h>
#include <Util/Util.h>

SelectorPanel::SelectorPanel(SingletonRepo* repo) :
        currentIndex(0)
    ,	indexDragged(0)
    ,	SingletonAccessor(repo, "SelectorPanel") {
    setRepaintsOnMouseActivity(true);
    setMouseCursor(MouseCursor::UpDownResizeCursor);

    itemName 		= "Layer";
    neverCollapsed 	= true;
    arrowImg 		= getObj(MiscGraphics).getIcon(6, 6);
}

int SelectorPanel::getExpandedSize() const {
    return 36;
}

int SelectorPanel::getCollapsedSize() const {
    return getExpandedSize();
}

void SelectorPanel::paint(Graphics& g) {
    Image topImg 	= arrowImg.getClippedImage(Rectangle<int>(0, 0, 11, 7));
    Image botImg 	= arrowImg.getClippedImage(Rectangle<int>(0, 7, 11, 7));

    int candidate 	= currentIndex + indexDragged;
    int listSize 	= getSize();

    bool smallCnd 	= candidate > 8;
    bool smallList 	= listSize > 9;

    String text 	 = String(candidate + 1);
    String totalText = String(listSize);

    Font font(FontOptions(smallCnd ? 11 : 13));
    int width = Util::getStringWidth(font, text);

    g.setFont(font);
    g.setColour(Colour::greyLevel(smallCnd ? 0.8f : 0.74f));
    g.drawSingleLineText(text, (getWidth() - width) / 2 + (smallCnd ? -6 : smallList ? -4 : 1), getHeight() / 2 + 3);
    g.setFont(*getObj(MiscGraphics).getSilkscreen());

    g.setColour(Colour::greyLevel(0.45f));
    g.drawSingleLineText(totalText, getWidth() - (smallCnd ? 11 : smallList ? 12 : 7), getHeight() / 2 + 3);

    g.setColour(Colours::white);
    g.drawImageAt(topImg, (getWidth() - topImg.getWidth()) / 2, 0);
    g.drawImageAt(botImg, (getWidth() - topImg.getWidth()) / 2, nextRect.getY());
}

void SelectorPanel::mouseEnter(const MouseEvent& e) {
    int listSize 	= getSize();
    String message 	= itemName + " " + String(currentIndex + 1) + " of " + String(listSize);

    getObj(IConsole).updateAll(message, String(), MouseUsage(true, true, false, false));
}

void SelectorPanel::mouseDrag(const MouseEvent& e) {
    if (e.mods.isLeftButtonDown()) {
        int listSize = getSize();
        draggedListIndex(-(e.getDistanceFromDragStartY() * listSize) / 60);
    }
}

void SelectorPanel::mouseDown(const MouseEvent& e) {
    if (e.mods.isLeftButtonDown()) {
        currentIndex = getCurrentIndexExternal();
    } else if (e.mods.isRightButtonDown()) {
        menu.clear();

        for (int i = 0; i < getSize(); ++i) {
            menu.addItem(i + 1, String(i + 1), true, false);
        }

        // TODO what happened to menu.show()?
        menu.showMenuAsync(PopupMenu::Options());
        int id = 0;
        if (id > 0) {
            currentIndex = id - 1;
            selectionChanged();
            repaint();
        }
    }
}

void SelectorPanel::mouseUp(const MouseEvent& e) {
    doSelectionChange();
}

void SelectorPanel::clickedOnRow(int row) {
    constrainIndex(row);
    currentIndex = row;

    rowClicked(row);
    selectionChanged();
    repaint();
}

void SelectorPanel::resized() {
    prevRect = Rectangle(1, 1, getWidth() - 1, getHeight() / 4);
    nextRect = Rectangle(1, 3 * getHeight() / 4 - 2, getWidth() - 1, getHeight() / 4);
}

void SelectorPanel::setBoundsDelegate(int x, int y, int w, int h) {
    setBounds(x, y, w, h);
}

Rectangle<int> SelectorPanel::getBoundsInParentDelegate() const {
    return getBoundsInParent();
}

int SelectorPanel::getXDelegate() {
    return getX();
}

int SelectorPanel::getYDelegate() {
    return getY();
}

void SelectorPanel::refresh(bool forceUpdate) {
    int oldCurrent = currentIndex;
    currentIndex = getCurrentIndexExternal();
    repaint();

    if (oldCurrent != currentIndex || forceUpdate) {
        selectionChanged();
    }
}

void SelectorPanel::constrainIndex(int& num) {
    int maxIndex = getSize() - 1;
    NumberUtils::constrain(num, 0, maxIndex);
}

void SelectorPanel::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) {
    float yInc = wheel.deltaY;

    draggedListIndex(yInc > 0 ? 1 : -1);
    doSelectionChange();
}

void SelectorPanel::draggedListIndex(int index) {
    indexDragged = index;

    int listSize = getSize();

    if (currentIndex + indexDragged > listSize - 1) {
        indexDragged = listSize - 1 - currentIndex;
    }

    if (currentIndex + indexDragged < 0) {
        indexDragged = -currentIndex;
    }

    showConsoleMsg(String("Current ") + itemName + " " + String(currentIndex + indexDragged + 1));

    repaint();
}

void SelectorPanel::doSelectionChange() {
    if (indexDragged == 0) {
        return;
    }

    currentIndex += indexDragged;
    constrainIndex(currentIndex);

    indexDragged = 0;

    selectionChanged();
}

void SelectorPanel::reset() {
    currentIndex = 0;
    repaint();
}

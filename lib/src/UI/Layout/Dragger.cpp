#include "Dragger.h"
#include "PanelPair.h"
#include "../IConsole.h"
#include "../../App/AppConstants.h"
#include "../../App/Settings.h"
#include "../../App/SingletonRepo.h"
#include "../../Definitions.h"


Dragger::Dragger(SingletonRepo* repo, int bitfield) : SingletonAccessor(repo, "Dragger") {
    this->bitfield = bitfield;
}


void Dragger::mouseDown(const MouseEvent& e) {
    listeners.call(&Listener::dragStarted);

    if (type == Horz) {
        startY = getScreenY() - getConstant(TitleBarHeight);
        startAH = pair->one->getHeight();
        startBH = pair->two->getHeight();
        startBY = pair->two->getY();
    } else if (type == Vert) {
        startX = getScreenX();
        startAW = pair->one->getWidth();
        startBW = pair->two->getWidth();
        startBX = pair->two->getX();
    }
}


void Dragger::mouseEnter(const MouseEvent& e) {
    repo->getConsole().updateAll(String(), String(), MouseUsage(true));
}


void Dragger::mouseDrag(const MouseEvent& e) {
    update(type == Horz ? e.getDistanceFromDragStartY() : e.getDistanceFromDragStartX());
}


void Dragger::mouseUp(const MouseEvent& e) {
    listeners.call(&Listener::dragEnded);
}


void Dragger::paint(Graphics& g) {
    g.setColour(Colour::greyLevel(0.1f));
    if (type == Horz) {
        g.fillRect(0, 2, getWidth(), getHeight() - 4);
        g.drawImageAt(dots, (getWidth() - dots.getWidth()) / 2, getHeight() / 2);
    } else {
        g.fillRect(2, 0, getWidth() - 6, getHeight());
        g.drawImageAt(dots, getWidth() / 2 - 1, (getHeight() - dots.getHeight()) / 2);
    }
}


void Dragger::update(int diff) {
    float newRatio;
    newRatio = (type == Horz) ?
               (startAH + diff) / float(startAH + startBH) :
               (startAW + diff) / float(startAW + startBW);

    pair->setPortion(newRatio);
    pair->resized();
}


void Dragger::resized() {
    dots = type == Horz ? Image(Image::RGB, 80, 1, true) : Image(Image::RGB, 1, 80, true);

    Graphics gfx(dots);
    gfx.setColour(Colour::greyLevel(0.08f));
    gfx.fillRect(dots.getBounds());

    Image::BitmapData data(dots, Image::BitmapData::readWrite);

    if (type == Horz) {
        int widthInc = 3;
        for (int i = 0; i < dots.getWidth() / widthInc - 1; ++i) {
            data.setPixelColour(i * widthInc + 1, 0, Colour::greyLevel(0.15f));
            data.setPixelColour(i * widthInc + 2, 0, Colour::greyLevel(0.4f));
        }
    } else if (type == Vert) {
        int heightInc = 3;
        for (int i = 0; i < dots.getHeight() / heightInc - 1; ++i) {
            data.setPixelColour(0, i * heightInc + 1, Colour::greyLevel(0.15f));
            data.setPixelColour(0, i * heightInc + 2, Colour::greyLevel(0.4f));
        }
    }
}


void Dragger::setPanelPair(PanelPair* pair) {
    this->pair = pair;
    type = pair->sideBySide;

    setMouseCursor(type == Horz ? MouseCursor::UpDownResizeCursor : MouseCursor::LeftRightResizeCursor);
}

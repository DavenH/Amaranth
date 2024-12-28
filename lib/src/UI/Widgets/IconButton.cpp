#include "IconButton.h"

#include <Definitions.h>

#include <utility>

#include "../MiscGraphics.h"
#include "../IConsole.h"
#include "../../Util/Util.h"
#include "../../App/SingletonRepo.h"

IconButton::IconButton(int x, int y,
                       Listener* listener,
                       SingletonRepo* repo,
                       String overMsg,
                       String cmdMsg,
                       String naMsg)
    :   Button(String())
    ,   SingletonAccessor(repo, String())
    ,   mouseScrollApplicable(false)
    ,   applicable      (true)
    ,   isPowered       (false)
    ,   highlit         (false)
    ,   pendingNumber   (0)
    ,   collapsedSize   (24)
    ,   expandedSize    (24)
    ,   message         (std::move(overMsg))
    ,   keys            (std::move(cmdMsg))
    ,   naMessage       (std::move(naMsg)) {
    neut = getObj(MiscGraphics).getIcon(x, y);

    addListener(listener);
    setMouseCursor(MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(false);
}

IconButton::IconButton(Image image, SingletonRepo* repo)
    :   Button(String())
    ,   SingletonAccessor(repo, String())
    ,   applicable      (true)
    ,   highlit         (false)
    ,   isPowered       (false)
    ,   pendingNumber   (0)
    ,   collapsedSize   (24)
    ,   expandedSize    (24)
    ,   neut            (std::move(image)) {
    setMouseCursor(MouseCursor::PointingHandCursor);
}

IconButton::~IconButton() {
    neut = Image();
}

bool IconButton::setHighlit(bool highlit) {
    if (Util::assignAndWereDifferent(this->highlit, highlit)) {
        repaint();
        return true;
    }

    return false;
}

void IconButton::setMessages(String mouseOverMessage, String keys) {
    message = std::move(mouseOverMessage);
    this->keys = std::move(keys);
}

void IconButton::paintButton(Graphics& g, bool mouseOver, bool buttonDown) {
    Image copy = neut;

    if (applicable) {
        if(highlit) {
            getObj(MiscGraphics).drawHighlight(g, Rectangle<float>(0, 0, 24, 24));
        }

        getObj(MiscGraphics).applyMouseoverHighlight(g, copy, mouseOver, buttonDown, pendingNumber > 0);
    }

    if(! applicable) {
        g.setOpacity(0.4f);
    }

    g.drawImage(copy, (buttonSize - neut.getWidth()) / 2, (buttonSize - neut.getHeight()) / 2,
                neut.getWidth(), neut.getHeight(), 0, 0, neut.getWidth(), neut.getHeight());

    if (isPowered) {
        Rectangle<int> p = getLocalBounds().removeFromTop(6).removeFromLeft(6).reduced(2, 2);

        g.setColour(Colour::greyLevel(highlit ? 0.8f : 0.6f));
        g.fillEllipse(p.getX(), p.getY(), p.getWidth(), p.getHeight());
    }
}

void IconButton::paintOutline(Graphics& g, bool mouseOver, bool buttonDown) {
    g.setColour(Colours::black);
    Rectangle r(0, 0, getWidth() - 1, getHeight() - 1);
    getObj(MiscGraphics).drawCorneredRectangle(g, r, 4);
}

void IconButton::mouseEnter(const MouseEvent& e) {
    repo->getConsole().updateAll(keys, applicable ? message : naMessage,
                                 MouseUsage(true, false, mouseScrollApplicable, false));

    Button::mouseEnter(e);
}

void IconButton::mouseDrag(const MouseEvent& e) {
    if (e.mods.isRightButtonDown())
        return;

    if (applicable) {
        Button::mouseDrag(e);
    } else {
        showConsoleMsg(naMessage);
    }
}

void IconButton::mouseDown(const MouseEvent& e) {
    if (!e.mods.isLeftButtonDown()) {
        return;
    }

    if (applicable) {
        Button::mouseDown(e);
    } else {
        showConsoleMsg(naMessage);
    }
}

void IconButton::setApplicable(bool applicable) {
    if (applicable == this->applicable)
        return;

    this->applicable = applicable;
//  setEnabled(applicable);

    repaint();
}

#include "TwoStateButton.h"
#include "../MiscGraphics.h"
#include "../../Definitions.h"
#include "../../App/SingletonRepo.h"

TwoStateButton::TwoStateButton(int x1, int y1, int x2, int y2,
                               Listener* listener,
                               SingletonRepo* repo,
                               const String& msgA, const String& keyA,
                               const String& msgB, const String& keyB) :
   IconButton(x1, y1, listener, repo, msgA, keyA),
   state(First),
   secondMessage(msgB),
   secondKey(keyB) {
    second = getObj(MiscGraphics).getIcon(x2, y2);
}

TwoStateButton::~TwoStateButton() {
    second = Image();
}

void TwoStateButton::paintButton(Graphics& g, bool mouseOver, bool buttonDown) {
    Image& image = state == First ? neut : second;

    int x = (getWidth() - image.getWidth()) >> 1;
    int y = (getHeight() - image.getHeight()) >> 1;

    g.drawImage(image, x, y, image.getWidth(), image.getHeight(), 0, 0, neut.getWidth(), neut.getHeight());
}

void TwoStateButton::setState(int state) {
    this->state = state;
    message = state == First ? message : secondMessage;
    keys = state == First ? keys : secondKey;
    repaint();
}

void TwoStateButton::mouseDown(const MouseEvent& e) {
    if(! e.mods.isLeftButtonDown()) {
        return;
    }

    toggle();

    IconButton::mouseDown(e);
}

void TwoStateButton::toggle() {
    state = (state == First) ? Second : First;
    message = state == First ? message : secondMessage;
    keys = state == First ? keys : secondKey;

    repaint();
}

void TwoStateButton::setMessages(String one, String key1, String two, String key2) {
    message = one;
    secondMessage = two;
    keys = key1;
    secondKey = key2;
}

bool TwoStateButton::isFirstState() const {
    return state == First;
}

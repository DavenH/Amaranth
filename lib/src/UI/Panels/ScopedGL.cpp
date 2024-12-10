#include "ScopedGL.h"
#include "OpenGL.h"

ScopedElement::ScopedElement(int glConstant)
        : glElement(glConstant) {
    glBegin(glConstant);
}


ScopedElement::~ScopedElement() {
    glEnd();
}


ScopedDisable::ScopedDisable(int glConstant, bool active)
        : active(active), glConstant(glConstant) {
    if (active)
        glDisable(glConstant);
}

ScopedDisable::~ScopedDisable() {
    if (active)
        glEnable(glConstant);
}

ScopedEnable::ScopedEnable(int glConstant, bool active)
        : active(active), glConstant(glConstant) {
    if (active)
        glEnable(glConstant);
}

ScopedEnable::~ScopedEnable() {
    if (active)
        glDisable(glConstant);
}


ScopedEnableClientState::ScopedEnableClientState(int glElement, bool active) : glElement(glElement), active(active) {
    if (active)
        glEnableClientState(glElement);
}


ScopedEnableClientState::~ScopedEnableClientState() {
    if (active)
        glDisableClientState(glElement);
}


#pragma once

#include "../App/SingletonRepo.h"
#include "../App/SingletonAccessor.h"
#include "../Obj/Ref.h"

#include <UI/IConsole.h>

class MouseOverMessager :
        public MouseListener
    , 	public SingletonAccessor {
public:

    MouseOverMessager(SingletonRepo* repo, const String& message, Component* comp) :
            SingletonAccessor(repo, "MouseOverMessager"), message(message) {
        comp->addMouseListener(this, true);
    }

    void mouseEnter(const MouseEvent& e) override {
        repo->getConsole().write(message);
    }


    String message;
};

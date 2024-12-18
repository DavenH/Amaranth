#pragma once

#include "Bounded.h"
#include "../../Obj/Ref.h"

class BoundWrapper : public Bounded {
    Ref<Component> component;

public:
    explicit BoundWrapper(Component* component) : component(component) {
    }

    void setBounds(int x, int y, int width, int height) override {
        component->setBounds(x, y, width, height);
    }

    const Rectangle<int> getBounds() override {
        return component->getBounds();
    }

    int getX() override {
        return component->getX();
    }

    int getY() override {
        return component->getY();
    }

    int getWidth() override {
        return component->getWidth();
    }

    int getHeight() override {
        return component->getHeight();
    }
};

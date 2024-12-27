#pragma once

#include "Bounded.h"
#include "../../Obj/Ref.h"
#include "../../Obj/Deletable.h"

class BoundWrapper
        : public Bounded
        , public Deletable {
    Ref<Component> component;

public:
    explicit BoundWrapper(Component* component)
        : Bounded(),
          component(component) {
    }

    void setBounds(int x, int y, int width, int height) override {
        component->setBounds(x, y, width, height);
    }

    [[nodiscard]] Rectangle<int> getBounds() const override {
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

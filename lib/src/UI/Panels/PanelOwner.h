#pragma once

#include "../../Obj/Ref.h"

template<class T>
class PanelOwner {
public:
    explicit PanelOwner(T* panel) : panel(panel) {
    }

    T* getPanel() { return panel; }

protected:
    Ref<T> panel;
};

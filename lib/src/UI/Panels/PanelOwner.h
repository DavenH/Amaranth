#ifndef PANEL2DOWNER_H_
#define PANEL2DOWNER_H_

#include "../../Obj/Ref.h"

template<class T>
class PanelOwner {
public:
    PanelOwner(T* panel) : panel(panel) {
    }

    T* getPanel() { return panel; }

protected:
    Ref<T> panel;
};


#endif

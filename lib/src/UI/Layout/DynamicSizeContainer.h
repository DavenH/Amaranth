#pragma once

#include "IDynamicSizeComponent.h"
#include "JuceHeader.h"
#include "../../Obj/Ref.h"

class DynamicSizeContainer : public IDynamicSizeComponent {
public:
    DynamicSizeContainer(Component* comp, int expandedSize, int collapsedSize) :
            comp(comp)
        ,	expandedSize(expandedSize)
        ,	collapsedSize(collapsedSize) {
    }

    void setBoundsDelegate(int x, int y, int w, int h) override { return comp->setBounds(x, y, w, h); }
    int getYDelegate() override 					    { return comp->getY(); 				}
    int getXDelegate() override 					    { return comp->getX(); 				}
    [[nodiscard]] Rectangle<int> getBoundsInParentDelegate() const override { return comp->getBoundsInParent(); }
    [[nodiscard]] int getExpandedSize() const override	{ return expandedSize; 				}
    [[nodiscard]] int getCollapsedSize() const override { return collapsedSize; 			}
    [[nodiscard]] bool isVisibleDlg() const override	{ return comp->isVisible(); 		}
    void setVisibleDlg(bool isVisible) override		    { comp->setVisible(isVisible); 		}

    Ref<Component> comp;
    int collapsedSize;
    int expandedSize;
};

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
	const Rectangle<int> getBoundsInParentDelegate() override { return comp->getBoundsInParent(); }
	int getYDelegate() override 					{ return comp->getY(); 				}
	int getXDelegate() override 					{ return comp->getX(); 				}
	int getExpandedSize() override					{ return expandedSize; 				}
	int getCollapsedSize() override 				{ return collapsedSize; 			}
	bool isVisibleDlg() const override				{ return comp->isVisible(); 		}
	void setVisibleDlg(bool isVisible) override		{ comp->setVisible(isVisible); 		}

	Ref<Component> comp;
	int collapsedSize;
	int expandedSize;
};

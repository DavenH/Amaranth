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

	void setBoundsDelegate(int x, int y, int w, int h)
										{ return comp->setBounds(x, y, w, h); }

	const Rectangle<int> getBoundsInParentDelegate()
										{ return comp->getBoundsInParent(); }

	int getYDelegate() 					{ return comp->getY(); 				}
	int getXDelegate() 					{ return comp->getX(); 				}
	int getExpandedSize() 				{ return expandedSize; 				}
	int getCollapsedSize() 				{ return collapsedSize; 			}
	bool isVisibleDlg() const 			{ return comp->isVisible(); 		}
	void setVisibleDlg(bool isVisible) 	{ comp->setVisible(isVisible); 		}

	Ref<Component> comp;
	int collapsedSize;
	int expandedSize;
};

#pragma once

#include <iostream>
#include "../Layout/IDynamicSizeComponent.h"
#include "../../App/SingletonAccessor.h"
#include "JuceHeader.h"

using std::cout;
using std::endl;

class IconButton :
		public Button
	,	public IDynamicSizeComponent
	,	public SingletonAccessor {
public:
    IconButton(Image image, SingletonRepo* repo);
    IconButton(int x, int y, Listener* listener,
    		   SingletonRepo* repo,
    		   const String& overMsg = String(),
    		   const String& cmdMsg = String(),
    		   const String& naMsg = String());

    virtual ~IconButton();
    virtual void paintButton(Graphics & g, bool mouseOver, bool buttonDown);

    void paintOutline(Graphics & g, bool mouseOver, bool buttonDown);
    void mouseEnter(const MouseEvent & e);
    void mouseDown(const MouseEvent& e);
    void mouseDrag(const MouseEvent& e);

    bool getIsPowered()										{ return isPowered; 			}
    bool isApplicable() const    							{ return applicable;    		}
    bool isHighlit() const       							{ return highlit;    			}
    bool isVisibleDlg() const								{ return isVisible();			}

    const Rectangle<int> getBoundsInParentDelegate() 		{ return getBoundsInParent();	}
    int getCollapsedSize()       							{ return collapsedSize;    		}
    int getExpandedSize()       							{ return expandedSize;    		}
    int getPendingItems()									{ return pendingNumber; 		}
    int getXDelegate()    									{ return getX();    			}
    int getYDelegate()   					 				{ return getY();    			}

    void setBoundsDelegate(int x, int y, int w, int h)		{ setBounds(x, y, w, h);		}
    void setCollapsedSize(int size)    						{ collapsedSize = size;    		}
    void setExpandedSize(int size)    						{ expandedSize = size;    		}
    void setMouseScrollApplicable(bool is) 					{ mouseScrollApplicable = is; 	}
    void setNotApplicableMessage(const String& naMessage)   { this->naMessage = naMessage;  }
    void setPendingItems(int is)    						{ pendingNumber	= is; repaint();}
    void setPowered(bool is) 								{ isPowered = is; repaint();	}
    bool setHighlit(bool highlit);
    void setMessages(String mouseOverMessage, String keys);
    void setApplicable(bool applicable);

protected:
    static const int buttonSize = 24;

    int expandedSize;
    int collapsedSize;
    int pendingNumber;

    bool applicable, highlit, isPowered;
    bool mouseScrollApplicable;

    String message;
    String keys;
    String naMessage;

    Image neut;

    JUCE_LEAK_DETECTOR(IconButton);
};
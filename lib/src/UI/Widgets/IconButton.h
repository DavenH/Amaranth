#pragma once

#include <iostream>
#include "../Layout/IDynamicSizeComponent.h"
#include "../../App/SingletonAccessor.h"
#include "JuceHeader.h"

using std::cout;
using std::endl;
using namespace juce;

class IconButton :
        public Button
    ,	public IDynamicSizeComponent
    ,	public SingletonAccessor {
public:
    IconButton(Image image, SingletonRepo* repo);
    IconButton(int x, int y, Listener* listener,
               SingletonRepo* repo,
               String overMsg = String(),
               String cmdMsg = String(),
               String naMsg = String());

    ~IconButton() override;
    void paintButton(Graphics & g, bool mouseOver, bool buttonDown) override;

    void paintOutline(Graphics & g, bool mouseOver, bool buttonDown);
    void mouseEnter(const MouseEvent & e) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;

    [[nodiscard]] bool getIsPowered() const						{ return isPowered; 			}
    [[nodiscard]] bool isApplicable() const    					{ return applicable;    		}
    [[nodiscard]] bool isHighlit() const       					{ return highlit;    			}
    [[nodiscard]] bool isVisibleDlg() const override			{ return isVisible();			}

    [[nodiscard]] Rectangle<int> getBoundsInParentDelegate() const override	{ return getBoundsInParent();	}
    [[nodiscard]] int getCollapsedSize() const override       	{ return collapsedSize;    		}
    [[nodiscard]] int getExpandedSize() const override       	{ return expandedSize;    		}
    [[nodiscard]] int getPendingItems() const					{ return pendingNumber; 		}
    int getXDelegate() override    								{ return getX();    			}
    int getYDelegate() override   					 			{ return getY();    			}
    void setBoundsDelegate(int x, int y, int w, int h) override	{ setBounds(x, y, w, h);		}

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
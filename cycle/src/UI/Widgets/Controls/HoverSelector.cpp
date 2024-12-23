#include <App/Doc/Document.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Definitions.h>
#include <UI/MiscGraphics.h>
#include <UI/Widgets/IconButton.h>
#include <Util/Util.h>
#include "HoverSelector.h"

HoverSelector::HoverSelector(SingletonRepo* repo, int x, int y, bool horz) :
		SingletonAccessor(repo, "HoverSelector")
	,	horizontal(horz) {
	Image img = getObj(MiscGraphics).getIcon(x, y);
    headerIcon = std::make_unique<IconButton>(img, repo);
	addAndMakeVisible(*headerIcon);

    headerIcon->addMouseListener(this, true);
    headerIcon->setMessages("Select mesh preset", {});

    menuActive = false;
}

void HoverSelector::resized() {
    headerIcon->setBounds(0, 0, 24, 24);
}

void HoverSelector::mouseEnter(const MouseEvent& e) {
    //	getSetting(LastPopupClickedHorz) = horizontal;
    //	showPopup();
}

void HoverSelector::mouseDown(const MouseEvent& e) {
    getSetting(LastPopupClickedHorz) = horizontal;
    getSetting(LastPopupClickedTransp) = true;

    showPopup();
}

void HoverSelector::mouseExit(const MouseEvent& e) {
}

//void HoverSelector::timerCallback(int id) {
//	if(id == ShowDelayId) {
//		if(window) {
//			if(window->isOverAnyMenu() || isMouseOver()) {
//				stopTimer(ShowDelayId);
//			} else {
//				PopupMenu::dismissAllActiveMenus();
//
//				stopTimer(ShowDelayId);
//				stopTimer(MouseOutId);
//				window = 0;
//			}
//		} else {
//			stopTimer(ShowDelayId);
//		}
//	}
//	else if(id == MouseOutId) {
//		if(window) {
//			if(window->isOverAnyMenu() || isMouseOver()) {
//				stopTimer(ShowDelayId);
//			} else {
//				startTimer(ShowDelayId, 300);
//			}
//		}
//	}
//}

void HoverSelector::setSelectedId(int id) {
    //	if(itemIsSelection(id)) {
    //		PopupMenu::dismissAllActiveMenus();
    //
    //		stopTimer(ShowDelayId);
    //		stopTimer(MouseOutId);
    //		window = 0;
    //	}

    itemWasSelected(id);
}

void HoverSelector::mouseOverItem(int itemIndex) {
}

void HoverSelector::mouseLeftItem(int itemIndex) {
}

void HoverSelector::paintOverChildren(Graphics& g) {
    Image pullout = getObj(MiscGraphics).getIcon(7, horizontal ? 6 : 7);

    g.setOpacity(1.f);
    g.drawImageAt(pullout, 0, 0);
}

class HoverSelectorCallback : public ModalComponentManager::Callback {
public:
    explicit HoverSelectorCallback(HoverSelector* const _panel) : panel(_panel) {
    }

    void modalStateFinished(int returnValue) override{
        if (panel != nullptr) {
            panel->menuActive = false;

            //			static int previousReturn = -1;

            if (returnValue == 0) {
                panel->revert();
            } else {
                //				if (previousReturn != returnValue)
                panel->setSelectedId(returnValue);
            }

            //			previousReturn = returnValue;
        }
    }

private:
    Component::SafePointer<HoverSelector> panel;
    HoverSelectorCallback(const HoverSelectorCallback&) = delete;
    HoverSelectorCallback& operator=(const HoverSelectorCallback&);
};

void HoverSelector::showPopup() {
    if (!menuActive) {
        menuActive = true;

        Rectangle<int> screenArea(getScreenBounds());

        //		if(horizontal)
        //		{
        //			screenArea.setX(screenArea.getX() + 24);
        //			screenArea.setY(screenArea.getY() - 24);
        //		}

        jassert(screenArea.getWidth() > 0);

        prepareForPopup();
        menu.showAt(screenArea, 1, getWidth(), 1, 24, new HoverSelectorCallback(this));
    }
}

#ifndef SELECTORPANEL_H_
#define SELECTORPANEL_H_

#include <App/Settings.h>
#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <Obj/Ref.h>
#include <UI/Layout/IDynamicSizeComponent.h>
#include "JuceHeader.h"

class SelectorPanel :
		public IDynamicSizeComponent
	,	public Component
	, 	public SingletonAccessor {
public:
	SelectorPanel(SingletonRepo* repo);
	virtual ~SelectorPanel();

	int getExpandedSize();
	int getCollapsedSize();
	void paint(Graphics& g);
	void resized();
	void clickedOnRow(int row);
	void setBoundsDelegate(int x, int y, int w, int h);
	const Rectangle<int> getBoundsInParentDelegate();
	int getYDelegate();
	int getXDelegate();

	void mouseDrag(const MouseEvent& e);
	void mouseUp(const MouseEvent& e);
	void mouseEnter(const MouseEvent& e);
	void mouseDown(const MouseEvent& e);
	void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel);
	void constrainIndex(int& num);
	void draggedListIndex(int index);

	void refresh(bool forceUpdate = false);
	bool isVisibleDlg() const { return isVisible(); }
	void setVisibleDlg(bool isVisible) { setVisible(isVisible); }
	void doSelectionChange();
	void reset();
	void setItemName(const String& name) { itemName = name; }
	int getCurrentIndex() { return currentIndex; }

	virtual int getSize() = 0;
	virtual int getCurrentIndexExternal() = 0;
	virtual void selectionChanged() = 0;
	virtual void rowClicked(int row) {}

	int currentIndex;

protected:
	int indexDragged;

	Rectangle<int> prevRect;
	Rectangle<int> nextRect;

	String itemName;

	PopupMenu menu;

private:
	GlowEffect blueGlow;
    GlowEffect redGlow;
    DropShadowEffect shadow;

	Image arrowImg;
};


#endif

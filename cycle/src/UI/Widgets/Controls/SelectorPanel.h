#pragma once

#include <App/SingletonAccessor.h>
#include <UI/Layout/IDynamicSizeComponent.h>
#include "JuceHeader.h"

class SelectorPanel :
		public IDynamicSizeComponent
	,	public Component
	, 	public SingletonAccessor {
public:
	explicit SelectorPanel(SingletonRepo* repo);

	~SelectorPanel() override = default;

	int getExpandedSize() override;
	int getCollapsedSize() override;
	void paint(Graphics& g) override;
	void resized() override;
	void clickedOnRow(int row);
	void setBoundsDelegate(int x, int y, int w, int h) override;
	const Rectangle<int> getBoundsInParentDelegate() override;
	int getYDelegate() override;
	int getXDelegate() override;

	void mouseDrag(const MouseEvent& e) override;
	void mouseUp(const MouseEvent& e) override;
	void mouseEnter(const MouseEvent& e) override;
	void mouseDown(const MouseEvent& e) override;
	void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
	void constrainIndex(int& num);
	void draggedListIndex(int index);

	void refresh(bool forceUpdate = false);
	bool isVisibleDlg() const override { return isVisible(); }
	void setVisibleDlg(bool isVisible) override { setVisible(isVisible); }
	void doSelectionChange();
	void reset() override;
	void setItemName(const String& name) { itemName = name; }
	int getCurrentIndex() const { return currentIndex; }

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

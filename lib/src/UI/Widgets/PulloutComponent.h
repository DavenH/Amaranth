#pragma once

#include <vector>
#include "../MouseEventDelegator.h"
#include "../Layout/IDynamicSizeComponent.h"
#include "../../Obj/Ref.h"
#include "JuceHeader.h"
#include "../../App/SingletonAccessor.h"

using std::vector;

#define menuDelayMillis 400

class PulloutComponent;
class SingletonRepo;
class MiscGraphics;

class BoxComp :
		public Component
	,	public MouseEventDelegatee
	,	public Timer {
public:
	vector<Component*> buttons;

	BoxComp();
	~BoxComp();
	void paint(Graphics& g);
	void setHorizontal(bool isHorizontal);
	void mouseEnter(const MouseEvent& e);
	void mouseExit(const MouseEvent& e);
	void addAll(vector<Component*>& _buttons);

	void timerCallback();
	void visibilityChanged();
	void setPulloutComponent(PulloutComponent* pullout);

	void enterDlg();
	void exitDlg();

	void setBackgroundOpacity(float opacity) { backgroundOpacity = opacity; }

private:
	bool horz;
	float backgroundOpacity;
	Ref<PulloutComponent> pullout;
	MouseEventDelegator delegator;

	JUCE_LEAK_DETECTOR(BoxComp)
};

class PulloutComponent :
		public IDynamicSizeComponent
	,	public Component
	,	public SingletonAccessor {
private:
	Image img;
	BoxComp popup;

	Ref<MiscGraphics> miscGraphics;
	bool horz;

	JUCE_LEAK_DETECTOR(PulloutComponent)

public:
	PulloutComponent(Image image, vector<Component*>& buttons, SingletonRepo* repo, bool horz = true);
	Image& getImage();
	void mouseEnter(const MouseEvent& e);
	void mouseExit(const MouseEvent& e);
	void moved();
	void paint(Graphics& g);
	void removeBoxFromDesktop();
	void removedFromDesktop();
	void resized();
	void setBackgroundOpacity(float opacity) { popup.setBackgroundOpacity(opacity); }

	int getExpandedSize() 				{ return 0; }
	int getCollapsedSize() 				{ return 0; }
	void setBoundsDelegate(int x, int y, int w, int h) {}
	const Rectangle<int> getBoundsInParentDelegate() { return getBounds(); }
	int getYDelegate() 					{ return 0; }
	int getXDelegate() 					{ return 0; }
	bool isVisibleDlg() const 			{ return isVisible(); }
	void setVisibleDlg(bool isVisible) 	{ setVisible(isVisible); }
};
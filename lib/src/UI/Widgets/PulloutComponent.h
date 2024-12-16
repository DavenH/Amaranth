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
	~BoxComp() override = default;
	void paint(Graphics& g) override;
	void setHorizontal(bool isHorizontal);
	void mouseEnter(const MouseEvent& e) override;
	void mouseExit(const MouseEvent& e) override;
	void addAll(vector<Component*>& _buttons);

	void timerCallback() override;
	void visibilityChanged() override;
	void setPulloutComponent(PulloutComponent* pullout);

	void enterDlg() override;
	void exitDlg() override;

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
	Image img;
	BoxComp popup;

	Ref<MiscGraphics> miscGraphics;
	bool horz;

	JUCE_LEAK_DETECTOR(PulloutComponent)

public:
	PulloutComponent(Image image, vector<Component*>& buttons, SingletonRepo* repo, bool horz = true);
	Image& getImage();
	void mouseEnter(const MouseEvent& e) override;
	void mouseExit(const MouseEvent& e) override;
	void moved() override;
	void paint(Graphics& g) override;
	void removeBoxFromDesktop();
	void removedFromDesktop();
	void resized() override;
	void setBackgroundOpacity(float opacity) { popup.setBackgroundOpacity(opacity); }

	int getExpandedSize() override 				{ return 0; }
	int getCollapsedSize() override 			{ return 0; }
	void setBoundsDelegate(int x, int y, int w, int h) override {}
	const Rectangle<int> getBoundsInParentDelegate() override { return getBounds(); }
	int getYDelegate() override 				{ return 0; }
	int getXDelegate() override 				{ return 0; }
	bool isVisibleDlg() const override 			{ return isVisible(); }
	void setVisibleDlg(bool isVisible) override { setVisible(isVisible); }
};
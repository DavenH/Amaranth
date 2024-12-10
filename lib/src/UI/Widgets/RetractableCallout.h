#pragma once
#include <vector>

#include "PulloutComponent.h"
#include "../Layout/IDynamicSizeComponent.h"
#include "../../Obj/Ref.h"
#include "JuceHeader.h"

using std::vector;

class RetractableCallout :
	public IDynamicSizeComponent,
	public Component {
public:
	RetractableCallout(const vector<Component*>& _icons, PulloutComponent* _pullout, bool _horz);
	virtual ~RetractableCallout();
	void paint(Graphics& g);
	void paintOverChildren(Graphics& g);
	void resized();
	void moved();
	bool isCollapsed();
	void setAlwaysCollapsed(bool shouldBe);

	int getExpandedSize();
	int getCollapsedSize();
	void setBoundsDelegate(int x, int y, int w, int h);
	const Rectangle<int> getBoundsInParentDelegate();
	int getYDelegate();
	int getXDelegate();
	bool isVisibleDlg() const { return isVisible(); }
	void setVisibleDlg(bool isVisible) { setVisible(isVisible); }

private:
	bool horz;
	bool showHeader;

	String name;
	vector<Component*> icons;
	Ref<PulloutComponent> pullout;

	JUCE_LEAK_DETECTOR(RetractableCallout)
};
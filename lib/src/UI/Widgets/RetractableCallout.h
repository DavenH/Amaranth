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

	~RetractableCallout() override = default;
	void paint(Graphics& g) override;
	void paintOverChildren(Graphics& g) override;
	void resized() override;
	void moved() override;
	bool isCollapsed();
	void setAlwaysCollapsed(bool shouldBe);

	[[nodiscard]] int getExpandedSize() const override;
	[[nodiscard]] int getCollapsedSize() const override;
	void setBoundsDelegate(int x, int y, int w, int h) override;
	[[nodiscard]] Rectangle<int> getBoundsInParentDelegate() const override;
	int getYDelegate() override;
	int getXDelegate() override;
	[[nodiscard]] bool isVisibleDlg() const override { return isVisible(); }
	void setVisibleDlg(bool isVisible) override { setVisible(isVisible); }

private:
	bool horz;
	bool showHeader{};

	String name;
	vector<Component*> icons;
	Ref<PulloutComponent> pullout;

	JUCE_LEAK_DETECTOR(RetractableCallout)
};
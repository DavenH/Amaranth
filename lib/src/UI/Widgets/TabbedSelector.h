#pragma once

#include <vector>
#include "../IConsole.h"
#include "../../App/SingletonAccessor.h"
#include "Obj/Deletable.h"

using std::vector;

class Bounded;
class InsetLabel;

class TabbedSelector :
		public Component
	,	public Deletable
	,	public SingletonAccessor {
public:
	explicit TabbedSelector(SingletonRepo* repo);
	void paint(Graphics& g) override;
	void mouseExit(const MouseEvent& e) override;
	void mouseMove(const MouseEvent& e) override;
	void mouseDown(const MouseEvent& e) override;
	void selectTab(int tab);
	void setSelectedTab(int tab);
	void callListeners(Bounded* selected);
	void addTab(const String& name, Bounded* callbackComponent, const String& keys = String());
	void resized() override;
	[[nodiscard]] int getSelectedId() const { return selectedTab; }

    class Listener {
	public:
	    virtual ~Listener() = default;

	    virtual void tabSelected(TabbedSelector* selector, Bounded* callbackComponent) = 0;
	};

	void addListener(Listener* listener);

private:
	void updateTabUnderMouse(const MouseEvent& e);

	class Tab {
	public:
		String name, shortcutKey;
		Bounded* callbackComponent;
		InsetLabel* label;
	};

	static const int radius = 5;
	static const int vertSpace = 5;

	int hoveredTab;
	int selectedTab;
	int tabWidth;
	int tabHeight;

	GlowEffect glow;
	OwnedArray<InsetLabel> labels;
	Array<Listener*> listeners;
	vector<Tab> tabs;

	JUCE_LEAK_DETECTOR(TabbedSelector)
};


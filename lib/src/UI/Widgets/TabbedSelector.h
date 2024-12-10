#pragma once

#include <vector>
#include "../IConsole.h"
#include "../../App/SingletonAccessor.h"
#include "JuceHeader.h"

using std::vector;

class Bounded;
class InsetLabel;

class TabbedSelector :
		public Component
	,	public SingletonAccessor {
public:

	TabbedSelector(SingletonRepo* repo);
	void paint(Graphics& g);
	void mouseExit(const MouseEvent& e);
	void mouseMove(const MouseEvent& e);
	void mouseDown(const MouseEvent& e);
	void selectTab(int tab);
	void setSelectedTab(int tab);
	void callListeners(Bounded* selected);
	void addTab(const String& name, Bounded* callbackComponent, const String& keys = String());
	void resized();
	int getSelectedId() { return selectedTab; }

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


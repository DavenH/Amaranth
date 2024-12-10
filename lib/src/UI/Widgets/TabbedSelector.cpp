#include "TabbedSelector.h"
#include "../IConsole.h"
#include "../Widgets/InsetLabel.h"
#include "../../App/SingletonRepo.h"


TabbedSelector::TabbedSelector(SingletonRepo* repo) :
		SingletonAccessor(repo, "TabbedSelector")
	,	selectedTab(0)
	,	hoveredTab(0)
	,	tabHeight(1)
	,	tabWidth(30) {
}


void TabbedSelector::paint(Graphics& g) {
	g.fillAll(Colours::black);

	float halfPi = 0.5f * float(IPP_PI);
	float rad = radius;

	for (int i = 0; i < (int) tabs.size(); ++i)
	{
		float startY = vertSpace + i * tabHeight;
		float endY = startY + tabHeight - 2;
		float startX = 0;
		float endX = getWidth() - 2;

		Path path;

        if (i != selectedTab) {
			path.startNewSubPath(startX, startY);
			path.lineTo(endX - rad, startY);
			path.addCentredArc(endX - rad - 1, startY + rad, rad, rad, 0, 0, halfPi, false);
			path.lineTo(endX - 1, endY - rad);
			path.addCentredArc(endX - rad - 1, endY - rad, rad, rad, 0, halfPi, 2 * halfPi, false);
			path.lineTo(startX, endY);
			path.closeSubPath();

			g.setColour(Colour::greyLevel(i == hoveredTab ? 0.20f : 0.15f));
			g.fillPath(path);
		}
	}

    if (selectedTab >= 0) {
		float startY = vertSpace + selectedTab * tabHeight;
		float endY = startY + tabHeight - 2;

		Path path;

		float startX = 0;
		float endX = getWidth() - 2;

		path.startNewSubPath(startX, startY);

		if (selectedTab > 0)
			path.addCentredArc(startX + rad, startY - 2.f * rad, rad, 2 * rad, 0, 3.f * halfPi, 2 * halfPi, false);

		path.lineTo(endX - rad, startY);
		path.addCentredArc(endX - rad - 1, startY + rad, rad, rad, 0, 0, halfPi, false);
		path.lineTo(endX - 1, endY - rad);
		path.addCentredArc(endX - rad - 1, endY - rad, rad, rad, 0, halfPi, 2 * halfPi, false);
		path.lineTo(startX + rad, endY);

		if (selectedTab < tabs.size() - 1)
			path.addCentredArc(startX + rad, endY + 2 * rad, rad, 2 * rad, 0, 4 * halfPi, 3 * halfPi, false);
		else
			path.lineTo(startX, endY);

		path.lineTo(startX, getHeight());
		path.closeSubPath();

		g.setColour(Colour(0.60f, 0.35f, 0.25f, 1.f));
		g.fillPath(path);

		g.setColour(Colour(0.60f, 0.35f, 0.40f, 1.f));
		g.strokePath(path, PathStrokeType(1.0f), AffineTransform::translation(-0.5f, -0.5f));
	}

	Font font(14, Font::bold);
	g.setFont(font);

	float inset = 0.45f;

	/// text

    for (int i = 0; i < (int) tabs.size(); ++i) {
		Tab& tab 			= tabs[i];

		const String& name 	= tab.name.toUpperCase();
		int strLength 		= font.getStringWidth(name);
		int y 				= vertSpace + i * tabHeight;
		float scale 		= jmin(tabHeight - 20, strLength) / float(strLength);

		AffineTransform transform;

		transform = transform.rotated	(halfPi);
		transform = transform.scaled	(1.f, scale);
		transform = transform.translated(4, y); //getWidth() - 4

		Colour backColour = i == selectedTab ? Colour(0.60f, 0.2f, 0.5f, 1.f) :
				Colour::greyLevel(i == hoveredTab ? 0.4f : 0.3f);

	#define tempdef

	#ifdef tempdef //JUCE_MAC

		Image positive(Image::ARGB, tabHeight, getWidth(), true);
		Graphics temp(positive);

		Rectangle<int> r(0, 0, tabHeight, getWidth());

		float baseBrightness = 0.3f;
		tab.label->setColorHSV(i == selectedTab ? 0.45f : 0.f,
								baseBrightness + (i == selectedTab ? 0.28f : i == hoveredTab ? 0.18f : 0.f));
		tab.label->setBounds(r);
		tab.label->paint(temp);

		g.drawImageTransformed(positive, transform.translated(getWidth() - 6, 0));

 	 #else
		transform = transform.translated(0, (tabHeight - actualLength) / 2);

	  #ifdef JUCE_V2
		g.saveState();
		{
			g.setColour(backColour);
			g.addTransform(transform.translated(inset, -inset));
			g.drawSingleLineText(name, 0, 0, Justification::left);

			g.setColour(Colours::black);
			g.addTransform(AffineTransform::translation(-inset, inset));
			g.drawSingleLineText(name, 0, 0, Justification::left);
		}
		g.restoreState();

	  #else
		g.setColour(backColour);
		g.drawTextAsPath(name, transform.translated(inset, inset));

		g.setColour(Colours::black);
		g.drawTextAsPath(name, transform.translated(-inset, -inset));
	  #endif
	#endif
	}
}


void TabbedSelector::mouseExit(const MouseEvent& e) {
	hoveredTab = -1;
	repaint();
}


void TabbedSelector::mouseMove(const MouseEvent& e) {
	int lastHover = hoveredTab;
	updateTabUnderMouse(e);

	if(hoveredTab < 0)
		return;

	if(lastHover != hoveredTab) {
		repo->getConsole().updateAll(tabs[hoveredTab].shortcutKey,
								   tabs[hoveredTab].name,
								   MouseUsage(true, false, false, false));
		repaint();
	}
}


void TabbedSelector::updateTabUnderMouse(const MouseEvent& e) {
    hoveredTab = -1;

    for (int i = 0; i < (int) tabs.size(); ++i) {
		const Rectangle<int> r(0, i * tabHeight, getWidth(), tabHeight);

		if (r.contains(e.x, e.y)) {
			hoveredTab = i;
			break;
		}
	}
}


void TabbedSelector::mouseDown(const MouseEvent& e) {
	if(! e.mods.isLeftButtonDown())
		return;

	updateTabUnderMouse(e);

	selectedTab = hoveredTab;

	if(! isPositiveAndBelow(selectedTab, (int) tabs.size()))
		return;

	callListeners(tabs[selectedTab].callbackComponent);
	repaint();
}


void TabbedSelector::selectTab(int tab) {
	if(! isPositiveAndBelow(tab, (int) tabs.size()))
		return;

	selectedTab = tab;

	callListeners(tabs[selectedTab].callbackComponent);
	repaint();
}


void TabbedSelector::callListeners(Bounded* selected) {
	for(int i = 0; i < (int) listeners.size(); ++i)
		listeners[i]->tabSelected(this, selected);
}


void TabbedSelector::addTab(const String& name, Bounded* callbackComponent, const String& keys) {
	Tab tab;
	tab.name = name.toUpperCase();
	tab.label = new InsetLabel(repo, tab.name);
	tab.label->setOffsets(-0.8f, 0.8f);
	tab.callbackComponent = callbackComponent;
	tab.shortcutKey = keys;

	labels.add(tab.label);
	tabs.push_back(tab);
}


void TabbedSelector::resized() {
	tabWidth = 0.85 * getWidth() + 0.5f;
	tabHeight = tabs.empty() ? 1 : (getHeight() - vertSpace * 2) / (float)tabs.size() - 0.5f;
}


void TabbedSelector::addListener(Listener* listener) {
	listeners.add(listener);
}


void TabbedSelector::setSelectedTab(int tab) {
	jassert(isPositiveAndBelow(tab, (int) tabs.size()));

	selectedTab = tab;

	repaint();
}

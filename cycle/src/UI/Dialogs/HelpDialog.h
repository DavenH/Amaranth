#pragma once

#include <App/AppConstants.h>
#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <UI/MiscGraphics.h>
#include "JuceHeader.h"

#include "../../Binary/CycleImages.h"
#include "../CycleGraphicsUtils.h"
#include "../../Util/CycleEnums.h"
#include "../../CycleDefs.h"

class HelpDialog :
		public juce::Component
	, 	public Button::Listener
	, 	public SingletonAccessor
{
public:
	explicit HelpDialog(SingletonRepo* repo) :
			SingletonAccessor(repo, "HelpDialog")
		,	closeButton	("Close")
	{
		closeButton.addListener(this);
		addAndMakeVisible(&closeButton);

		logo 		= PNGImageFormat::loadFrom(CycleImages::cyclelogo_png, CycleImages::cyclelogo_pngSize);
//		beatLogo	= PNGImageFormat::loadFrom(CycleImages::beatlogo_png,  CycleImages::beatlogo_pngSize );
		bert 		= PNGImageFormat::loadFrom(CycleImages::cyclebert_png, CycleImages::cyclebert_pngSize);
	}

	void paint(Graphics& g) override {
		getObj(CycleGraphicsUtils).fillBlackground(this, g);

		Font font(FontOptions(16));
		g.setFont(font);

		g.setColour(Colour::greyLevel(0.75f));
		Rectangle<int> r = getLocalBounds();
		r.reduce(50, 20);

		Rectangle<int> r2 = r.removeFromTop(120);

		g.drawImageWithin(logo, r2.getX(), r2.getY(), r2.getWidth(), r2.getHeight(),
		                  RectanglePlacement::centred | RectanglePlacement::doNotResize, false);

		r.reduce(20, 0);

		String name("Cycle");
		String version(ProjectInfo::versionString);
		String line(name + " v" + version + " build " + String(getConstant(BuildNumber)));
		getObj(MiscGraphics).drawCentredText(g, r.removeFromTop(25), line, Justification::centred);

		r.removeFromTop(20);
		getObj(MiscGraphics).drawCentredText(g, r.removeFromTop(25), "by Amaranth Audio, 2024", Justification::centred);

//		g.drawImageAt(bert, 0, 0);
		g.drawImageTransformed(bert, AffineTransform::rotation(-0.5).translated(getWidth() * 0.65, getHeight() * 0.7));
	}

	void resized() override {
		Rectangle<int> r = getLocalBounds();

		Rectangle<int> bottom = r.removeFromBottom(60);
		bottom.reduce(bottom.getWidth() / 2 - 60, 12);

		closeButton.setBounds(bottom);
	}

	void buttonClicked(Button* button) override {
		removeFromDesktop();
		setVisible(false);

		delete this;
	}

	Image logo, bert;
	TextButton closeButton;
};

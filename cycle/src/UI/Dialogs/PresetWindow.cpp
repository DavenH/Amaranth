#include <App/SingletonRepo.h>
#include <Definitions.h>

#include "PresetWindow.h"
#include "PresetPage.h"

PresetWindow::PresetWindow()
		: DialogWindow("Preset Browser", Colour::greyLevel(0.08f), true, false)
{
	setResizable(true, true);
}

void PresetWindow::show(SingletonRepo* repo, Component* toCentreAround) {
	if (!JUCEApplication::isStandaloneApp()) {
		setAlwaysOnTop(true);
	}

	setContentNonOwned(&getObj(PresetPage), true);
	centreAroundComponent(toCentreAround, getWidth(), getHeight());
	setVisible(true);
}

void PresetWindow::closeButtonPressed()
{
	setVisible(false);
}

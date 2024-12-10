#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/PluginProcessor.h>
#include <UI/Widgets/CalloutUtils.h>
#include <UI/Widgets/IconButton.h>

#include "ResizerPullout.h"
#include "../PluginWindow.h"

#if PLUGIN_MODE
  using namespace WindowSizes;
#endif

ResizerPullout::ResizerPullout(SingletonRepo* repo) :
		SingletonAccessor(repo, "ResizerPullout")
	,	smallIcon(	new IconButton(8, 1, this, repo, "50% size"))
	,	medIcon(	new IconButton(8, 2, this, repo, "70% size"))
	,	fullIcon( 	new IconButton(8, 3, this, repo, "100% size"))
	,	keybIcon( 	new IconButton(8, 4, this, repo, "Player Mode")) {
    Component* icons[] = {keybIcon, smallIcon, medIcon, fullIcon};

    medIcon->setHighlit(true);

    vector < Component * > buttons;
    std::copy(icons, icons + numElementsInArray(icons), inserter(buttons, buttons.begin()));

	pullout = new PulloutComponent(getObj(MiscGraphics).getIcon(8, 0), buttons, repo, false);
	pullout->setBackgroundOpacity(1.f);
	pullout->setBounds(0, 0, 24, 24);
	addAndMakeVisible(pullout);
}


void ResizerPullout::buttonClicked(Button* button) {
#if PLUGIN_MODE
    PluginWindow* window = dynamic_cast<PluginWindow*>(getObj(PluginProcessor).getActiveEditor());

    int oldSize = getSetting(WindowSize);

    int size =
            button == smallIcon ? SmallSize :
            button == medIcon 	? MedSize 	:
            button == fullIcon 	? FullSize 	:
                                  PlayerSize;

    updateHighlight(size);

    pullout->removeBoxFromDesktop();

    if(oldSize != size)
        window->changeSizeAndSet(size);
#endif
}


void ResizerPullout::resized() {
}


void ResizerPullout::updateHighlight(int windowSize) {
#if PLUGIN_MODE
    smallIcon->setHighlit(windowSize == SmallSize);
    medIcon	 ->setHighlit(windowSize == MedSize	);
    fullIcon ->setHighlit(windowSize == FullSize);
    keybIcon ->setHighlit(windowSize == PlayerSize);
#endif
}


#pragma once

#include <vector>
#include <iterator>
#include <algorithm>
#include "../../Definitions.h"

#include "RetractableCallout.h"
#include "IconButton.h"
#include "TwoStateButton.h"
#include "../MiscGraphics.h"
#include "../../App/SingletonRepo.h"

class CalloutUtils {
public:
	static void addRetractableCallout(
	        RetractableCallout*& callout,
	        PulloutComponent*& pullout,
	        SingletonRepo* repo,
	        int posX, int posY,
	        Component** buttonArray,
	        int numButtons,
	        Component* parent,
	        bool horz = false) {

		vector<Component*> buttons;
		std::copy(buttonArray, buttonArray + numButtons, inserter(buttons, buttons.begin()));
		MiscGraphics& mg = getObj(MiscGraphics);
		pullout = new PulloutComponent(mg.getIcon(posX, posY), buttons, repo, !horz);
		callout = new RetractableCallout(buttons, pullout, horz);

		parent->addAndMakeVisible(callout);
	}

	static void addRetractableCallout(
	        ScopedPointer<RetractableCallout>& callout,
	        ScopedPointer<PulloutComponent>& pullout,
	        SingletonRepo* repo,
	        int posX, int posY,
	        Component** buttonArray,
	        int numButtons,
	        Component* parent,
	        bool horz = false) {

		vector<Component*> buttons;
		std::copy(buttonArray, buttonArray + numButtons, inserter(buttons, buttons.begin()));

		MiscGraphics& mg = getObj(MiscGraphics);
		pullout = new PulloutComponent(mg.getIcon(posX, posY), buttons, repo, !horz);
		callout = new RetractableCallout(buttons, pullout, horz);

		parent->addAndMakeVisible(callout);
	}

};

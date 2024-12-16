#pragma once

#include <memory>
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
		auto& mg = getObj(MiscGraphics);
		pullout = new PulloutComponent(mg.getIcon(posX, posY), buttons, repo, !horz);
		callout = new RetractableCallout(buttons, pullout, horz);

		parent->addAndMakeVisible(callout);
	}

	static void addRetractableCallout(
	        std::unique_ptr<RetractableCallout>& callout,
	        std::unique_ptr<PulloutComponent>& pullout,
	        SingletonRepo* repo,
	        int posX, int posY,
	        Component** buttonArray,
	        int numButtons,
	        Component* parent,
	        bool horz = false) {

		vector<Component*> buttons;
		std::copy(buttonArray, buttonArray + numButtons, inserter(buttons, buttons.begin()));

		auto& mg = getObj(MiscGraphics);
		pullout = std::make_unique<PulloutComponent>(mg.getIcon(posX, posY), buttons, repo, !horz);
		callout = std::make_unique<RetractableCallout>(buttons, pullout.get(), horz);

		parent->addAndMakeVisible(callout.get());
	}

};

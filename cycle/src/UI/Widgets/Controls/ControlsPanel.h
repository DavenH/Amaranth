#pragma once

#include <set>
#include <algorithm>
#include <map>
#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include "JuceHeader.h"

using std::set;
using std::map;
using std::pair;

class IconButton;
class PulloutComponent;
class RetractableCallout;
class IDynamicSizeComponent;
class MiscGraphics;

class ImageContainer {
public:
	int x, y;
	Image image;
};

class ControlsPanel :
		public Component
	,	public SingletonAccessor {
public:
	ControlsPanel(Button::Listener* parent, SingletonRepo* repo, bool fillBackground = false);
	~ControlsPanel() override;
	void paint(Graphics& g) override;

	void addRetractableCallout(
			std::unique_ptr<RetractableCallout>& callout,
			std::unique_ptr<PulloutComponent>& pullout,
			SingletonRepo* repo,
			int posX, int posY,
			Component** buttonArray, int numButtons, bool horz = false);

	void resized() override;
	void addBand(IDynamicSizeComponent* end);
	void addDividedBand(IDynamicSizeComponent* start, IDynamicSizeComponent* end);
	void addDynamicSizeComponent(IDynamicSizeComponent* component, bool manage = true);
	void addButton(IconButton* button);
	void setComponentVisibility(int index, bool isVisible);

	void orphan();
//	ControlsPanel& operator=(const ControlsPanel& copy);

protected:
	bool isHorz;
	bool fillBackground;

	Array<ImageContainer> images;
	Array<IDynamicSizeComponent*> expandableComponents;
	Array<IDynamicSizeComponent*> componentsToDelete;
	Array<int> spacers;
	Button::Listener* parent;

	Array<IDynamicSizeComponent*> bandOffsets;
	Array<IDynamicSizeComponent*> bands;
	Array<pair<IDynamicSizeComponent*, IDynamicSizeComponent*> > dividedBands;
	map<IDynamicSizeComponent*, int> bandCount;
};
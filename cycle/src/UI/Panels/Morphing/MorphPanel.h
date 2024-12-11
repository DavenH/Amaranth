#ifndef _MorphPanel_h
#define _MorphPanel_h

#include <App/Doc/Savable.h>
#include <App/SingletonAccessor.h>
#include <Definitions.h>
#include <Inter/MorphPositioner.h>
#include <Obj/MorphPosition.h>
#include <UI/AsyncUIUpdater.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/TwoStateButton.h>

#include "../../TourGuide.h"
#include "../../Widgets/HSlider.h"
#include "../../Widgets/MorphSlider.h"

class PulloutComponent;
class RetractableCallout;
class CubeDisplay;
class VertCube;

class MorphPanel :
		public Component
	,	public Slider::Listener
	,	public Button::Listener
	,	public AsyncUIUpdater
	,	public Savable
	,	public TourGuide
	,	public SingletonAccessor
	,	public MorphPositioner {
public:
	MorphPanel(SingletonRepo* repo);
	virtual ~MorphPanel();
	void init();

	/* UI */
	void paint(Graphics& g);
	void resized();
	void showMessage(float value, MorphSlider* slider);

	void reduceDetail();
	void restoreDetail();
	void doGlobalUIUpdate(bool force);

	/*Events */
	void buttonClicked(Button* button);
	void sliderDragEnded(Slider* slider);
	void sliderDragStarted(Slider* slider);
	void sliderValueChanged(Slider* slider);
	void setSelectedCube(Vertex* vert, VertCube* cube, int scratchChannel, bool isEnvelope);
	void setRedBlueStrings(const String& redStr, const String& blueStr);
	void updateModPosition(int dim, float value);
	void updateCurrentSliderNoCallback(float value);
	void updateHighlights();
	void updateCube();
	void triggerValue(int dim, float value);
	void triggerClick(int button);

	/* Accessors */
	void 	redDimUpdated(float value);
	void 	blueDimUpdated(float value);

	void 	setDepth(int dim, float depth);
	void 	setKeyValueForNote(int midiNote);
	void 	setViewDepth(int dim, float depth);
	void 	setValue(int dim, float value);

	bool 	usesLineDepthFor(int dim);

	int 	getPrimaryDimension();
	int 	getCurrentMidiKey();

	float 	getValue(int index);
	float 	getPrimaryViewDepth();
	float 	getDepth(int dim);
	float 	getDistortedTime(int chan);

	float 	getYellow() 		{ return yllwSlider.getValue(); }
	float 	getRed() 			{ return redSlider.getValue();  }
	float 	getBlue() 			{ return blueSlider.getValue(); }

	Slider* getPanSlider() 		{ return &panSlider;  }
	Slider* getMorphSlider()	{ return &blueSlider; }
	Slider* getKeySlider() 		{ return &redSlider;  }
	Slider* getTimeSlider() 	{ return &yllwSlider; }

	Component* getComponent(int which);

	MorphPosition getMorphPosition();
	MorphPosition getOffsetPosition(bool includeDepths);

	/* Persistence */
	void writeXML(XmlElement* element) const;
	bool readXML(const XmlElement* element);

private:
	bool ignoreNextEditMessage;

	HSlider 	panSlider;
	MorphSlider yllwSlider;
	MorphSlider redSlider;
	MorphSlider blueSlider;

	// dim buttons
	IconButton primeYllw;
	IconButton primeRed;
	IconButton primeBlue;

	IconButton linkYllw;
	IconButton linkRed;
	IconButton linkBlue;

	IconButton rangeYllw;
	IconButton rangeRed;
	IconButton rangeBlue;

	Component slidersArea;

	std::unique_ptr<CubeDisplay> cubeDisplay;
	std::unique_ptr<PulloutComponent> dimPO;
	std::unique_ptr<RetractableCallout> dimCO;

	std::unique_ptr<PulloutComponent> rangePO;
	std::unique_ptr<RetractableCallout> rangeCO;

	std::unique_ptr<PulloutComponent> linkPO;
	std::unique_ptr<RetractableCallout> linkCO;

	Rectangle<int> cubeBounds;
	Range<int> midiRange;

	float viewDepth[5];
	float insertDepth[5];
};

#endif

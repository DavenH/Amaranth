#pragma once

#include <App/Doc/Savable.h>
#include <App/SingletonAccessor.h>
#include <Inter/MorphPositioner.h>
#include <Obj/MorphPosition.h>
#include <UI/AsyncUIUpdater.h>
#include <UI/Widgets/IconButton.h>

#include "../../TourGuide.h"
#include "../../Widgets/HSlider.h"
#include "../../Widgets/MorphSlider.h"

class PulloutComponent;
class RetractableCallout;
class CubeDisplay;
class VertCube;

class MorphPanel :
        public juce::Component
    ,	public Slider::Listener
    ,	public Button::Listener
    ,	public AsyncUIUpdater
    ,	public Savable
    ,	public TourGuide
    ,	public SingletonAccessor
    ,	public MorphPositioner {
public:
    explicit MorphPanel(SingletonRepo* repo);
    void init() override;

    /* UI */
    void paint(Graphics& g) override;
    void resized() override;
    void showMessage(float value, MorphSlider* slider);

    void reduceDetail() override;
    void restoreDetail() override;
    void doGlobalUIUpdate(bool force) override;

    /*Events */
    void buttonClicked(Button* button) override;
    void sliderDragEnded(Slider* slider) override;
    void sliderDragStarted(Slider* slider) override;
    void sliderValueChanged(Slider* slider) override;
    void setSelectedCube(Vertex* vert, VertCube* cube, int scratchChannel, bool isEnvelope);
    void setRedBlueStrings(const String& redStr, const String& blueStr);
    void updateModPosition(int dim, float value);
    void updateCurrentSliderNoCallback(float value);
    void updateHighlights();
    void updateCube() const;
    void triggerValue(int dim, float value);
    void triggerClick(int button);

    /* Accessors */
    void 	redDimUpdated(float value);
    void 	blueDimUpdated(float value);

    void 	setDepth(int dim, float depth);
    void 	setKeyValueForNote(int midiNote);
    void 	setViewDepth(int dim, float depth);
    void 	setValue(int dim, float value);

    bool 	usesLineDepthFor(int dim) override;

    int 	getPrimaryDimension() override;
    int 	getCurrentMidiKey();

    float 	getValue(int index) override;
    float 	getPrimaryViewDepth();
    float 	getDepth(int dim) override;
    float 	getDistortedTime(int chan) override;

    float 	getYellow() override { return yllwSlider.getValue(); }
    float 	getRed() override 	 { return redSlider.getValue();  }
    float 	getBlue() override 	 { return blueSlider.getValue(); }

    Slider* getPanSlider() 		{ return &panSlider;  }
    Slider* getMorphSlider()	{ return &blueSlider; }
    Slider* getKeySlider() 		{ return &redSlider;  }
    Slider* getTimeSlider() 	{ return &yllwSlider; }

    Component* getComponent(int which) override;

    MorphPosition getMorphPosition() override;
    MorphPosition getOffsetPosition(bool includeDepths) override;

    /* Persistence */
    void writeXML(XmlElement* element) const override;
    bool readXML(const XmlElement* element) override;

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

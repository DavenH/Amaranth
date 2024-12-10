#ifndef _fourier3d_h
#define _fourier3d_h

#include <list>
#include <vector>
#include <iostream>

#include <App/MeshLibrary.h>
#include <Audio/SmoothedParameter.h>
#include <Obj/ColorGradient.h>
#include <Obj/Ref.h>
#include <UI/AsyncUIUpdater.h>
#include <UI/Panels/Panel3D.h>
#include <UI/ParameterGroup.h>
#include <UI/Widgets/DynamicLabel.h>
#include <UI/Widgets/IconButton.h>

#include "../TourGuide.h"
#include "../Widgets/Controls/ControlsClient.h"
#include "../Widgets/Controls/LayerAddRemover.h"
#include "../Widgets/Controls/LayerSelectionClient.h"
#include "../Widgets/Controls/LayerUpDownMover.h"
#include "../Widgets/Controls/PanelControls.h"
#include "../Widgets/Controls/Spacers.h"

using std::list;
using std::vector;

class RetractableCallout;
class PulloutComponent;
class Knob;
class Spectrum2D;
class HSlider;

template<class Mesh> class MeshSelector;

class Spectrum3D:
		public Panel3D
	,	public Panel3D::DataRetriever
	,	public Button::Listener
	,	public Slider::Listener
	,	public ParameterGroup::Worker
	,	public LayerSelectionClient
	,	public ControlsClient
	,	public AsyncUIUpdater
	,	public TourGuide
	,	public Savable {
public:
	enum LayerMode { Additive, Subtractive };

	Spectrum3D(SingletonRepo* repo);
	~Spectrum3D();
	void init();

	/* UI */
	void drawPlaybackLine();

	void reset();
	void panelResized();
	bool haveAnyValidLayers(bool isMags, bool haveAnyValidTimeLayers);

	/* Events */
	void layerChanged();
	void updateColours();
	void populateLayerBox();
	void enablementsChanged();
	bool validateScratchChannels();
	void setIconHighlightImplicit();
	void changedToOrFromTimeDimension();
	void triggerButton(int button);
	void scratchChannelSelected(int channel);
	void updateBackground(bool onlyVerticalBackground);
	void modeChanged(bool isMags, bool updateInteractors);

	void buttonClicked(Button* button);
	void sliderValueChanged(Slider* slider);

	/* Knob Events */
	bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate);
	bool shouldTriggerGlobalUpdate(Slider* slider);
	void restoreDetail();
	void reduceDetail();
	void doGlobalUIUpdate(bool force);
	void updateKnobValue();
	void updateSmoothedParameters(int deltaSamples);
	void updateScratchComboBox();
	void updateLayerSmoothedParameters(int voiceIndex, int deltaSamples);
	void updateSmoothParametersToTarget(int voiceIndex);
	void setLayerParameterSmoothing(int voiceIndex, bool smooth);

	/* Persistence */
	void writeXML(XmlElement* element) const;
	bool readXML(const XmlElement* element);

	/* Accessors */
	void setPan(int layerIdx, bool isFreq, float value);
	void setDynRange(int layerIdx, bool isFreq, float value);
	int getLayerScratchChannel();
	int getNumActiveLayers();

	Buffer<float> getColumnArray();
	const vector<Column>& getColumns();

	MeshLibrary::Properties* getPropertiesForLayer(int layerIdx, bool isFreq);
	MeshLibrary::Properties* getCurrentProperties();
	MeshLibrary::LayerGroup& getCurrentGroup();

	Component* 			getComponent(int which);
	vector<Color>& 		getGradientColours();
	int 				getLayerType();

	PanelControls* 		getPanelControls() 	{ return panelControls; }
	int& 				getScaleFactor() 	{ return scaleFactor; 	}
	CriticalSection& 	getLayerLock() 		{ return layerLock; 	}

	static float calcPhaseOffsetScale(float value) 	{ return expf(5.f * value); }
	static float calcDynamicRangeScale(float value) { return powf(2, 12 * value - 4); }

private:
	enum Knobs { Pan, DynRange };

	int scaleFactor;

	ClearSpacer spacer6;
	CriticalSection layerLock;
	ColorGradient 	phaseGradient;

	IconButton phaseIcon;
	IconButton magsIcon;
	IconButton additiveIcon;
	IconButton subtractiveIcon;

	StringFunction dynStr;

	Ref<HSlider> dynRangeKnob;
	Ref<HSlider> layerPan;
	Ref<MeshLibrary> meshLib;
	Ref<Spectrum2D> spect2D;

	ScopedPointer<PulloutComponent> 	operPO;
	ScopedPointer<RetractableCallout> 	operCO;

	ScopedPointer<PulloutComponent> 	modePO;
	ScopedPointer<RetractableCallout> 	modeCO;
	ScopedPointer<MeshSelector<Mesh> > 	meshSelector;
};

#endif

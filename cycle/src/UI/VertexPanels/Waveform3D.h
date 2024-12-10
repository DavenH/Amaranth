#ifndef _surface_h
#define _surface_h

#include <iostream>
#include <App/Doc/Savable.h>
#include <Audio/SmoothedParameter.h>
#include <UI/AsyncUIUpdater.h>
#include <UI/Panels/Panel3D.h>
#include <UI/Panels/ZoomPanel.h>
#include <UI/ParameterGroup.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/Knob.h>
#include "../TourGuide.h"
#include "../Widgets/Controls/ControlsClient.h"
#include "../Widgets/Controls/LayerAddRemover.h"
#include "../Widgets/Controls/LayerSelectionClient.h"
#include "../Widgets/Controls/Spacers.h"
#include "../Widgets/Controls/PanelControls.h"

using std::cout;
using std::endl;

#ifndef GL_BGRA_EXT
	 #define GL_BGRA_EXT 0x80e1
#endif

class EditWatcher;
class Mesh;
class WaveformInter3D;
template<class Mesh> class MeshSelector;

class Waveform3D:
		public Panel3D
	,	public Panel3D::DataRetriever
	,	public Slider::Listener
	,	public Button::Listener
	,	public ParameterGroup::Worker
	,	public LayerSelectionClient
	,	public ControlsClient
	,	public AsyncUIUpdater
	,	public TourGuide
	,	public Savable {
public:
	Waveform3D(SingletonRepo*);
	virtual ~Waveform3D();

	void init();

	/* UI */
	void panelResized();
	void reset();

	/* Events */
	bool shouldDrawGrid();
	bool shouldTriggerGlobalUpdate(Slider* slider);
	bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate);
	bool validateScratchChannels();

	Component* getComponent(int which);

	int getLayerScratchChannel();
	int getNumActiveLayers();

	void doGlobalUIUpdate(bool force);
	void layerChanged();
	void populateLayerBox();
	void reduceDetail();
	void restoreDetail();
	void scratchChannelSelected(int channel);
	void setLayerParameterSmoothing(int voiceIndex, bool smooth);
	void triggerButton(int button);
	void updateLayerSmoothedParameters(int voiceIndex, int deltaSamples);
	void updateScratchComboBox();
	void updateSmoothedParameters(int deltaSamples);
	void updateSmoothParametersToTarget(int voiceIndex);
	void zoomUpdated();
	void doZoomExtra(bool commandDown);

	void buttonClicked(Button* button);
	void sliderValueChanged (Slider* slider);

	Buffer<float> getColumnArray();
	const vector<Column>& getColumns();

	/* Accessors */
	int 				getLayerType();
	int 				getWindowWidthPixels();

	CriticalSection& 	getLayerLock() { return layerLock; }

	void setPan(int layerIdx, float unitValue);
	void setKnobValuesImplicit();

	/* Persistence */
	bool readXML(const XmlElement* element);
	void writeXML(XmlElement* element) const;

private:
	MeshLibrary::Properties* getCurrentProperties();

	enum Knobs { Pan, Fine };

	IconButton model;
	IconButton deconvolve;
	ClearSpacer spacer8;

	CriticalSection layerLock;

	Ref<HSlider> layerFine;
	Ref<HSlider> layerPan;
	Ref<WaveformInter3D> surfInteractor;
	ScopedPointer<MeshSelector<Mesh> > 	meshSelector;
};

#endif

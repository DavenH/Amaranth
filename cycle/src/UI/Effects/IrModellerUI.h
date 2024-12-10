#ifndef _tubemodelcomponent_h
#define _tubemodelcomponent_h

#include <App/Doc/Savable.h>
#include <Obj/Ref.h>
#include <Obj/ColorGradient.h>
#include <UI/ParameterGroup.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/InsetLabel.h>
#include <Util/StringFunction.h>

#include "../TourGuide.h"
#include "../Widgets/Controls/ControlsClient.h"
#include "../Widgets/Controls/ControlsPanel.h"
#include "../Widgets/Controls/Spacers.h"
#include "../Widgets/Controls/TitleBackingCont.h"
#include "../Widgets/Controls/PanelControls.h"
#include "../VertexPanels/EffectPanel.h"
#include "../../Audio/Effects/IrModeller.h"
#include "../../UI/Widgets/Controls/MeshSelectionClient.h"

template<class Mesh> class MeshSelector;
class PulloutComponent;
class RetractableCallout;
class IrModeller;
class ConvTest;
class SingletonRepo;
class Knob;

class IrModellerUI:
		public Savable
	,	public TourGuide
	,	public ControlsClient
	,	public Button::Listener
	,	public EffectPanel
	,	public ParameterGroup::Worker
	,	public MeshSelectionClient<Mesh>
{
public:
	IrModellerUI(SingletonRepo* repo);
	~IrModellerUI();

	void init();
	void initControls();

	/* Accessors */
	String getKnobName(int index) const;
	bool isEffectEnabled() const { return isEnabled; }
	bool isCurrentMeshActive()	 { return isEnabled; }
	int getLayerType() 			 { return layerType; }
	void setEffectEnabled(bool is);

	/* UI */
	void preDraw();
	void postCurveDraw();
	void panelResized();
	void showCoordinates();
	void doGlobalUIUpdate(bool force);
	void doLocalUIUpdate();
	void reduceDetail();
	void restoreDetail();
	void updateDspSync();
	bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate);
	bool shouldTriggerGlobalUpdate(Slider* slider);
	bool paramTriggersAggregateUpdate(int knobIndex);

	/* Events */
	void buttonClicked(Button* button);
    void setWaveImpulsePath(const String& filename) { waveImpulsePath = filename; }
	void modelLoadedWave();
	void loadWaveFile();
	void deconvolve();

	/* Persistence */
	void writeXML(XmlElement* element) const;
	bool readXML(const XmlElement* element);

	/* Mesh selection */
	Mesh* getCurrentMesh();
	void setCurrentMesh(Mesh* mesh);
	void previewMesh(Mesh* mesh);
	void previewMeshEnded(Mesh* oldMesh);
	void setMeshAndUpdate(Mesh* mesh, bool repaint = true);
	void setMeshAndUpdateNoRepaint(Mesh* mesh);
	void exitClientLock();
	void enterClientLock();
	void doubleMesh();
	void doZoomAction(int action);

	void createScales();
	void drawScales();

	/* Tutorial */
	Component* getComponent(int which);

private:
	bool isEnabled;

	Ref<IrModeller> irModeller;
	ScopedPointer<MeshSelector<Mesh> > selector;

	String waveImpulsePath;
	ColorGradient gradient;

	IconButton openWave;
	IconButton removeWave;
	IconButton modelWave;

	IconButton impulseMode;
	IconButton freqMode;
	IconButton attkZoomIcon;
	IconButton zoomOutIcon;

	Label bufSizeLabel;
	Label nameLabel;
	Label nameLabelA;
	Label labels[IrModeller::numTubeParams];

	Ref<HSlider> lengthSlider;
	Ref<HSlider> gainSlider;
	Ref<HSlider> hpSlider;

	ClearSpacer spacer8, spacer2;

	ScopedPointer<PulloutComponent> 	wavePO;
	ScopedPointer<PulloutComponent> 	zoomPO;
	ScopedPointer<PulloutComponent> 	modePO;

	ScopedPointer<RetractableCallout> 	waveCO;
	ScopedPointer<RetractableCallout> 	zoomCO;
	ScopedPointer<RetractableCallout> 	modeCO;

	InsetLabel title;
	TitleBacking titleBacking;

	friend class ConvTest;

};

#endif

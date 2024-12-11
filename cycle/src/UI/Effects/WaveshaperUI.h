#ifndef _waveshapercomponent_h
#define _waveshapercomponent_h

#include <App/Doc/Savable.h>
#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/ParameterGroup.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/InsetLabel.h>
#include <Util/StringFunction.h>

#include "../TourGuide.h"
#include "../Widgets/Controls/ControlsPanel.h"
#include "../Widgets/Controls/TitleBackingCont.h"
#include "../VertexPanels/EffectPanel.h"
#include "../Widgets/Controls/MeshSelectionClient.h"
#include "../../Audio/Effects/WaveShaper.h"
#include "../../Util/CycleEnums.h"

template<class Mesh> class MeshSelector;

class WaveshaperUI :
		public EffectPanel
	,	public MeshSelectionClient<Mesh>
	,	public Button::Listener
	,	public ComboBox::Listener
	,	public ParameterGroup::Worker
	,	public Savable
	,	public TourGuide
{
public:
	WaveshaperUI(SingletonRepo* repo);
	~WaveshaperUI();

	void init();

	/* Accessors */
	String getKnobName(int index) const;
	void setEffectEnabled(bool enabled);
	bool isEffectEnabled() const 		{ return isEnabled; }
	bool isCurrentMeshActive() 			{ return isEnabled; }
	ControlsPanel* getControlsPanel() 	{ return &controls; }

	/* UI */
	void preDraw();
	void postCurveDraw();
	void showCoordinates();
	void panelResized();


	void doGlobalUIUpdate(bool force);
	void reduceDetail();
	void restoreDetail();
	bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate);
	void updateDspSync();
	void doLocalUIUpdate();
	bool shouldTriggerGlobalUpdate(Slider* slider);

	/* Events */
	void buttonClicked(Button* button);
	void comboBoxChanged(ComboBox* box);
	void exitClientLock();
	void enterClientLock();

	/* Persistence */
	void writeXML(XmlElement* element) const;
	bool readXML(const XmlElement* element);

	/* Mesh selection */
	void setMeshAndUpdate	(Mesh* mesh);
	void setCurrentMesh		(Mesh* mesh);
	void previewMesh		(Mesh* mesh);
	void previewMeshEnded	(Mesh* oldMesh);
	void doubleMesh();

	Mesh* getCurrentMesh();
	Component* getComponent(int which);
	CriticalSection& getClientLock();

	int getUpdateSource() 			{ return UpdateSources::SourceWaveshaper; }
	int getLayerType() 				{ return layerType; }

private:
	static const int oversampFactors[5];

	bool isEnabled;

	InsetLabel 		title;
	ControlsPanel 	controls;
	TitleBacking 	titleBacking;
	IconButton 		enabledButton;
	ComboBox 		oversampleBox;

	Ref<Waveshaper> waveshaper;
	std::unique_ptr<MeshSelector<Mesh> > selector;


	Label nameLabel;
	Label nameLabelA;
	Label oversampLabel;
	Label labels[Waveshaper::numWaveshaperParams];

};

#endif

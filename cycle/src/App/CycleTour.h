#ifndef TUTORIALRUNNER_H_
#define TUTORIALRUNNER_H_

#include <vector>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Curve/Vertex.h>
#include <Curve/Vertex2.h>
#include <Obj/Ref.h>
#include "JuceHeader.h"
#include <Util/CommonEnums.h>

#include "../Audio/Effects/Unison.h"
#include "../UI/Panels/SynthMenuBarModel.h"
#include "../UI/TourGuide.h"
#include "../Util/CycleEnums.h"

using std::vector;

class Panel;
class Interactor;

class CycleTour :
		public Component
	,	public AsyncUpdater
	,	public MultiTimer
	,	public SingletonAccessor

{
public:

	enum Area
	{
		AreaNull			,
		AreaWshpEditor		,
		AreaSharpBand		,
		AreaWfrmWaveform3D	,
		AreaSpectrum		,
		AreaSpectrogram		,
		AreaEnvelopes		,
		AreaVolume			,
		AreaPitch			,
		AreaScratch			,
		AreaWaveshaper		,
		AreaDeformers		,
		AreaImpulse			,
		AreaMorphPanel		,
		AreaVertexProps		,
		AreaGenControls		,
		AreaConsole			,
		AreaPlayback		,
		AreaUnison			,
		AreaReverb			,
		AreaDelay			,
		AreaEQ				,
		AreaMain			,
		AreaModMatrix		,
		AreaMasterCtrls		,
	};

	enum ActionType
	{
		NullAction 			,
		SetNoteOn 			,
		SetNoteOff 			,
		StartPlayback		,

		SwitchMode			,
		SwitchToEnv			,
		SwitchToTool		,

		ShowArea			,
		HideArea			,

		SetDeclick			,
		SetMorphRange		,
		SetLayerMode		,
		SetPlaybackPos		,
		SetKnobValue		,
		SetVertexParam		,
		SetMatrixCell		,
		SetLayer			,
		SetPropsValue		,
		SetVertexSize		,
		SetUseRange			,
		SetViewStage		,

		LoadRefSample		,
		UnloadSample		,
		ChangeViewMode		,

		ImpLoadWave			,
		ImpModelWave		,
		ImpUnloadWave		,

		Zoom				,
		DeformLine			,

		Enable				,
		Disable				,

		LinkRange			,
		UnlinkRange			,

		TriggerButton		,
		OpenFactoryPreset 	,

		SetAxeSize			,
		ChopLines			,
		AddPoint			,
		DeletePoint			,
		MovePoint			,
		SelectPoint			,
		SelectVertAtPos		,
		DeselectPoints		,
	};

	enum CondOper
	{
		OperNull,
		OperEquals,
		OperLessThan,
		OperLessThanEQ,
		OperMoreThan,
		OperMoreThanEQ,
	};

	enum Comparable
	{
		NoCompare,
		NumPoints,
		NumLines,
		NumLayers,
		CurrentLayer,
		DfrmAssignments,
	};

	enum
	{
		TargNull = -1,
		IdNull = -1
	};

	enum
	{
		IdOn	,
		IdOff	,
	};

	enum
	{
		IdModeMagn	,
		IdModePhase
	};

	enum
	{
		ToFrontId = 0xc0dedbad
	};

	enum
	{
		IdModeUniGroup	,
		IdModeUniSingle
	};

	enum
	{
		IdViewSynth,
		IdViewWave,
	};

	enum TargMatrix
	{
		TargMatrixGrid,
		TargMatrixUtility,
		TargMatrixSource,
		TargMatrixAddSource,
		TargMatrixAddDest,
		TargMatrixDest,
	};

	enum TargUni
	{
		TargUniMode = Unison::numParams,
		TargUniVoiceSlct,
		TargUniAddRemove
	};


	enum DfrmTargets
	{
		TargDfrmNoise,
		TargDfrmOffset,
		TargDfrmPhase,
		TargDfrmMeshSlct,
		TargDfrmLayerAdd,
		TargDfrmLayerSlct
	};

	enum ImpTargets
	{
		TargImpLength,
		TargImpGain,
		TargImpHP,
		TargImpZoom,
		TargImpLoadWav,
		TargImpUnloadWav,
		TargImpModelWav,
	};


	enum WaveshaperTargets
	{
		TargWaveshaperOvsp,
		TargWaveshaperPre,
		TargWaveshaperPost,
		TargWaveshaperSlct,
	};

	enum VertexProps
	{
		TargSliderArea,
		TargTimeSlider,
		TargPhsSlider,
		TargAmpSlider,
		TargKeySlider,
		TargMorphSlider,
		TargCrvSlider,

		TargBoxArea = 10,
		TargPhsBox,
		TargAmpBox,
		TargKeyBox,
		TargModBox,
		TargCrvBox,
		TargAvpBox,

		TargGainArea = 20,
		TargPhsGain,
		TargAmpGain,
		TargKeyGain,
		TargModGain,
		TargCrvGain,
		TargAvpGain,
	};

	enum CompEnvs
	{
		TargVol,
		TargScratch,
		TargPitch,
		TargWavPitch,
		TargScratchLyr,
		TargSustLoop,
	};

	enum CompWaveform3D
	{
		TargDomains,
		TargLayerEnable,
		TargLayerMode,
		TargLayerAdder,
		TargLayerMover,
		TargLayerSlct,
		TargScratchBox,
		TargDeconv,
		TargModelCycle,
		TargPhaseUp,
		TargPan,
		TargRange,
		TargMeshSelector
	};

	enum GenControls
	{
		TargSelector,
		TargPencil,
		TargAxe,
		TargNudge,
		TargWaveVerts,
		TargVerts,
		TargLinkYellow
	};

	enum MasterCtrls
	{
		TargMasterVol,
		TargMasterOct,
		TargMasterLen,
	};

	enum
	{
		IdZoomIn	,
		IdZoomOut	,
	};


	enum
	{
		IdToolSelector 	= Tools::Selector,
		IdToolPencil	= Tools::Pencil,
		IdToolAxe		= Tools::Axe,
	};

	enum
	{
		IdMeshVol		= LayerGroups::GroupVolume,
		IdMeshPitch		= LayerGroups::GroupPitch,
		IdMeshScratch	= LayerGroups::GroupScratch,
		IdMeshWavePitch	= LayerGroups::GroupWavePitch,
	};

	enum
	{
		IdYellow		= Vertex::Time,
		IdRed			= Vertex::Red,
		IdBlue			= Vertex::Blue,
	};

	enum
	{
		IdParamTime 	= Vertex::Time,
		IdParamAvp 		= Vertex::Time,
		IdParamPhase 	= Vertex::Phase,
		IdParamAmp 		= Vertex::Amp,
		IdParamBlue 	= Vertex::Red,
		IdParamRed 		= Vertex::Blue,
		IdParamSharp 	= Vertex::Curve,
	};

	enum
	{
		IdViewStageA	= SynthMenuBarModel::ViewStageA,
		IdViewStageB	= SynthMenuBarModel::ViewStageB,
		IdViewStageC	= SynthMenuBarModel::ViewStageC,
		IdViewStageD	= SynthMenuBarModel::ViewStageD,
	};

	enum ModComps
	{
		TargVertCube,

		TargPrimeArea,
		TargPrimeY,
		TargPrimeB,
		TargPrimeR,

		TargLinkArea,
		TargLinkY,
		TargLinkB,
		TargLinkR,

		TargRangeArea,
		TargRangeY,
		TargRangeB,
		TargRangeR,

		TargSlidersArea,
		TargSliderY,
		TargSliderB,
		TargSliderR,
		TargSliderPan,
	};


	enum
	{
		IdBttnEnable,
		IdBttnAdd,
		IdBttnRemove,
		IdBttnMoveUp,
		IdBttnMoveDown,
		IdBttnModeAdditive,
		IdBttnModeFilter,
		IdBttnDeconv,
		IdBttnModel,
		IdBttnPhaseUp,
		IdBttnLoop,
		IdBttnSustain,

		IdBttnLinkY,
		IdBttnLinkR,
		IdBttnLinkB,

		IdBttnRangeY,
		IdBttnRangeR,
		IdBttnRangeB,

		IdBttnPrimeY,
		IdBttnPrimeR,
		IdBttnPrimeB,
	};

	struct Action
	{
		bool		hasExecuted, triggersUpdate;
		int 		data1, data2, data3, id;
		int			delayMillis;
		float 		value;
		Area 		area;
		ActionType 	type;
		Vertex2 	point;
		String		str;

		Action() : 	type(NullAction), id(IdNull), area(AreaNull),
					hasExecuted(false), triggersUpdate(false), value(0), data1(0), data2(0), data3(0), delayMillis(100)
		{}
	};

/*
	struct Animation
	{
		bool hasExecuted;
		int delayMillis, timerId, currentIndex;
		vector<Action> actions;

		Animation() : hasExecuted(false), delayMillis(40), timerId(1), currentIndex(0) {}
	};
	*/

	struct Condition
	{
		int 		value;

		Area		area;
		CondOper 	oper;
		Comparable 	type;
		String 		failMsg;

		Condition() : value(0), type(NoCompare), oper(OperEquals) {}
	};

	struct Item
	{
		int 		subArea;
		int 		actionIndex;

		Area 		area;
		Condition 	condition;
//		Animation 	animation;
		vector<Action> actions;

		String title;
		String text;

		Item() : subArea(-1)
		{
		}

		Rectangle<int> getArea(CycleTour* tour) const
		{
			if(subArea != TargNull)
			{
				TourGuide* t = tour->getTourGuide(area);

				if(Component* c = t->getComponent(subArea))
					return c->getScreenBounds();
			}
			else
			{
				if(Component* c = tour->getComponent(area))
					return c->getScreenBounds();
			}

			return Rectangle<int>();
		}
	};


	class Highlighter : public Component
	{
	public:
		Highlighter(CycleTour* tour);

		void paint(Graphics& g);
		void update(const Rectangle<int>& r);

		CycleTour* tour;
	};

	class ItemComponent : public Component
	{
	public:
		void paint(Graphics& g);
		void setItem(const Item& item);

		TextLayout layout;
		Item item;
	};

	class ItemWrapper : public Component, public SingletonAccessor
	{
	public:
		ItemWrapper(SingletonRepo* repo);

		void paint(Graphics& g);
		void moved();
		void resized();
		void childBoundsChanged (Component*);

		void setItem(const Item& item);
		bool keyPressed(const KeyPress& press);
		void updatePosition (const Rectangle<int>& newAreaToPointTo,
							 const Rectangle<int>& newAreaToFitIn);

		bool intersects(const Rectangle<float>& r, const Line<float>& l);

	private:
		void refreshPath();

		int borderSpace;
		float arrowSize;

		Path outline;
		Image background;
		Point<float> targetPoint;
		Rectangle<int> availableArea, targetArea;
		ScopedPointer<ItemComponent> content;
	};

	CycleTour(SingletonRepo* repo);
	virtual ~CycleTour();

	void init();
	void exit();
	void enter();
	void showNext();
	void showPrevious();
	void handleAsyncUpdate();
	void timerCallback(int id);
	void performAction(Action& type);

	bool isLive() { return live; }
	bool conditionPassed(const Condition& c);
	bool compare(const Condition& cond, int value);
	bool passesRequirements(const String& ignore, const String& require);
	void readAction(Action& action, XmlElement* actionElem);
	bool readXML(const XmlElement* element);

	Panel* 		areaToPanel(int which);
	Interactor* areaToInteractor(int which);
	Component* 	getComponent(int which);
	TourGuide* 	getTourGuide(Area area);

	class Tutorial
	{
	public:
		Tutorial() {}
		Tutorial(const String& name) : name(name) {}
		String name;
		vector<Item> items;
	};


private:
	bool live, createdWaveshape;
	int currentItem, lastItem, currentTutorial;

	Tutorial 	current;
	Highlighter highlighter;
	ItemWrapper wrapper;

	HashMap<String, int> 		subareaStrings, idStrings;
	HashMap<String, Area> 		areaStrings;
	HashMap<String, Comparable> compareStrings;
	HashMap<String, ActionType> actionStrings;
	HashMap<String, CondOper> 	condStrings;
};

#endif

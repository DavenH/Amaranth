#include <iterator>
#include <Design/Updating/Updater.h>
#include <UI/IConsole.h>
#include <UI/Layout/Bounded.h>
#include <UI/Layout/BoundWrapper.h>
#include <UI/Layout/Dragger.h>
#include <UI/Layout/PanelPair.h>
#include <UI/MasterRenderer.h>
#include <UI/MiscGraphics.h>
#include <UI/Panels/OpenGLPanel.h>
#include <UI/Panels/OpenGLPanel3D.h>
#include <UI/Panels/ZoomPanel.h>
#include <Util/ScopedBooleanSwitcher.h>

#include "BannerPanel.h"
#include "DerivativePanel.h"
#include "GeneralControls.h"
#include "MainPanel.h"
#include "Morphing/MorphPanel.h"
#include "OscControlPanel.h"
#include "PlaybackPanel.h"
#include "SynthMenuBarModel.h"
#include "VertexPropertiesPanel.h"

#include "../Dialogs/PresetPage.h"
#include "../Effects/DelayUI.h"
#include "../Effects/EqualizerUI.h"
#include "../Effects/EqualizerUI.h"
#include "../Effects/IrModellerUI.h"
#include "../Effects/ReverbUI.h"
#include "../Effects/UnisonUI.h"
#include "../Effects/WaveshaperUI.h"
#include "../SynthLookAndFeel.h"
#include "../VertexPanels/DeformerPanel.h"
#include "../VertexPanels/EffectPanel.h"
#include "../VertexPanels/Envelope2D.h"
#include "../VertexPanels/Envelope3D.h"
#include "../VertexPanels/Spectrum2D.h"
#include "../VertexPanels/Spectrum3D.h"
#include "../VertexPanels/Waveform2D.h"
#include "../VertexPanels/Waveform3D.h"
#include "../VisualDsp.h"
#include "../Widgets/Controls/ControlsPanel.h"
#include "../Widgets/MidiKeyboard.h"

#include "../../App/Dialogs.h"
#include "../../App/KeyboardInputHandler.h"
#include "../../Audio/AudioSourceRepo.h"
#include "../../Audio/Effects/Delay.h"
#include "../../Audio/Effects/Equalizer.h"
#include "../../Audio/Effects/IrModeller.h"
#include "../../Audio/Effects/Reverb.h"
#include "../../Audio/Effects/Unison.h"
#include "../../Audio/Effects/WaveShaper.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Curve/GraphicRasterizer.h"
#include "../CycleDefs.h"


MainPanel::MainPanel(SingletonRepo* repo) : 
		ComponentMovementWatcher(this)
	,	SingletonAccessor	(repo, "MainPanel")
	,	WaveDragTarget		(repo)

	,	focusedPanel		(nullptr)
	,	needsRepaint		(true)
	,	gainedFocus			(false)
	,	initialized			(false)
	,	forceResize			(false)
	,	attachNextResize	(false)
	,	viewMode			(CollapsedView)

	,	cv_wholePortion		(0.36)
	,	cv_middlePortion	(0.60)
	,	cv_spectSurfPortion	(0.5)
	,	cv_surfEnvPortion	(0.75)

	,	xv_wholePortion		(0.3)
	,	xv_spectSurfPortion	(0.5)
	,	xv_dfrmImpPortion	(0.45)
	,	xv_topBttmPortion	(0.84)
	,	xv_envDfmImpPortion	(0.4) {
}


MainPanel::~MainPanel() {
    stopTimer(BoundsCheckId);
	stopTimer(DelayedRepaint);

	removeListeners();
}


void MainPanel::init() {
	delayUI			= &getObj(DelayUI);
	derivPanel		= &getObj(DerivativePanel);
	dfrmPanel		= &getObj(DeformerPanel);
	envelope2D 		= &getObj(Envelope2D);
	envelope3D		= &getObj(Envelope3D);
	eqUI			= &getObj(EqualizerUI);
	generalControls = &getObj(GeneralControls);
	irModelUI		= &getObj(IrModellerUI);
	keyboard		= &getObj(MidiKeyboard);
	morphPanel 		= &getObj(MorphPanel);
	playbackPanel	= &getObj(PlaybackPanel);
	presetPage		= &getObj(PresetPage);
	reverbUI		= &getObj(ReverbUI);
	spectrum2D 		= &getObj(Spectrum2D);
	spectrum3D 		= &getObj(Spectrum3D);
	unisonUI		= &getObj(UnisonUI);
	vtxPropsPanel	= &getObj(VertexPropertiesPanel);
	waveform2D 		= &getObj(Waveform2D);
	waveform3D 		= &getObj(Waveform3D);
	waveshaperUI	= &getObj(WaveshaperUI);
	console 		= dynamic_cast<Console*>(&getObj(IConsole));

	initialisePanels();
}

void MainPanel::tabSelected(TabbedSelector* selector, Bounded* callbackComponent) {
    if (selector == bottomTabs.get()) {
        toggleEffectsWaveform2DPresets(callbackComponent == wave2DPair.get() ? 0 : callbackComponent == ev_bottomRight.get() ? 1 : 2);
    } else if (selector == topTabs.get()) {
        toggleF2DGuide(callbackComponent == dfrmPair.get());
    } else if (selector == topRightTabs.get()) {
        toggleEnvPanel(callbackComponent == envelope3D->getZoomPanel());
    }
}

void MainPanel::initialisePanels() {
    keyboard->setLowestVisibleKey(2 * 12);
	this->setWantsKeyboardFocus(true);

	Component* spectCtrls  	= spectrum3D->getControlsComponent();
	Component* surfCtrls 	= waveform3D->getControlsComponent();
	Component* envCtrls  	= envelope2D->getControlsComponent();
	Component* dfrmCtrls	= dfrmPanel	->getControlsComponent();
	Component* irmodCtrls 	= irModelUI	->getControlsComponent();
	Component* wshpCtrls 	= waveshaperUI->getControlsPanel();

	bottomTabs			= std::make_unique<TabbedSelector>(repo);
	topTabs				= std::make_unique<TabbedSelector>(repo);
	topRightTabs		= std::make_unique<TabbedSelector>(repo);

	oscCtrlBounds	 	= std::make_unique<BoundWrapper>(&getObj(OscControlPanel));
	menuBar 			= std::make_unique<MenuBarComponent>(&getObj(SynthMenuBarModel));
	bannerPanel			= std::make_unique<BannerPanel>(repo);

	cv_middleDragger 	= std::make_unique<Dragger>(repo, Dragger::ReduceNothing);
	cv_envSpectDragger 	= std::make_unique<Dragger>(repo, Dragger::ReduceF3dDetail);
	cv_wholeDragger 	= std::make_unique<Dragger>(repo, Dragger::ReduceSurfDetail | Dragger::ReduceF3dDetail);
	cv_spectSurfDragger = std::make_unique<Dragger>(repo, Dragger::ReduceSurfDetail | Dragger::ReduceF3dDetail);

	consBounds 			= std::make_unique<BoundWrapper>(console);
	modBounds 			= std::make_unique<BoundWrapper>(morphPanel);
	keybBounds 			= std::make_unique<BoundWrapper>(keyboard);
	playbackBounds 		= std::make_unique<BoundWrapper>(playbackPanel);
	genBounds 			= std::make_unique<BoundWrapper>(generalControls);

	surfCtrlBounds		= std::make_unique<BoundWrapper>(surfCtrls);
	spectCtrlBounds		= std::make_unique<BoundWrapper>(spectCtrls);
	guideCtrlBounds		= std::make_unique<BoundWrapper>(dfrmCtrls);
	tubeCtrlBoundsB		= std::make_unique<BoundWrapper>(irmodCtrls);
	envCtrlBounds		= std::make_unique<BoundWrapper>(envCtrls);
	wsCtrlBounds		= std::make_unique<BoundWrapper>(wshpCtrls);

	derivBounds 		= std::make_unique<BoundWrapper>(derivPanel);
	propsBounds 		= std::make_unique<BoundWrapper>(vtxPropsPanel);
	unisonBounds 		= std::make_unique<BoundWrapper>(unisonUI);
	reverbBounds		= std::make_unique<BoundWrapper>(reverbUI);
	delayBounds			= std::make_unique<BoundWrapper>(delayUI);
	eqBounds			= std::make_unique<BoundWrapper>(eqUI);
	bnrBounds 			= std::make_unique<BoundWrapper>(bannerPanel.get());
	menuBounds 			= std::make_unique<BoundWrapper>(menuBar.get());
	presetPageBounds	= std::make_unique<BoundWrapper>(presetPage);
	cv_botTabBounds		= std::make_unique<BoundWrapper>(bottomTabs.get());
	cv_topTabBounds		= std::make_unique<BoundWrapper>(topTabs.get());

	Bounded* envBounds = getSetting(CurrentMorphAxis) == Vertex::Time ? envelope2D->getZoomPanel() : envelope3D->getZoomPanel();

	int cpWidth   = 64;
	int tubeWidth = 64;

	wave2DPair	= new PanelPair(repo, derivBounds, 		waveform2D->getZoomPanel(),		false, 	0.01f, 	"wave2Dpair", 	xsBord, 5, 5);
	spectPair	= new PanelPair(repo, spectCtrlBounds,	spectrum3D->getZoomPanel(),		true, 	0.06f, 	"spectpair", 	xsBord, cpWidth, cpWidth);
	wavePair	= new PanelPair(repo, surfCtrlBounds, 	waveform3D->getZoomPanel(),		true, 	0.06f, 	"surfpair",  	xsBord, cpWidth, cpWidth);
	envPair		= new PanelPair(repo, envCtrlBounds, 	envBounds,  					true, 	0.06f, 	"envpair", 	 	xsBord, cpWidth, cpWidth);
	dfrmPair	= new PanelPair(repo, guideCtrlBounds,	dfrmPanel->getZoomPanel(),		true, 	0.03f, 	"guidepair", 	xsBord, cpWidth, cpWidth);
	irmodPair	= new PanelPair(repo, tubeCtrlBoundsB,	irModelUI->getZoomPanel(), 		true, 	0.03f, 	"tubepairb",	xsBord, tubeWidth, tubeWidth);
	wshpPair	= new PanelPair(repo, wsCtrlBounds, 	waveshaperUI->getZoomPanel(), 	false, 	0.03f, 	"wavepair",  	xsBord, 36, 36);

	panelGroups.add(&wave2DGroup);
	panelGroups.add(&surfGroup);
	panelGroups.add(&spectGroup2);
	panelGroups.add(&spectGroup3);
	panelGroups.add(&envGroup2);
	panelGroups.add(&envGroup3);
	panelGroups.add(&wshpGroup);
	panelGroups.add(&irGroup);
	panelGroups.add(&dfrmGroup);

	initialiseMainView();
	initialiseFXView();
	initialiseExtendedView();

	String cmdStr("ctrl");

  #ifdef JUCE_MAC
	cmdStr = L"\u2318";
  #endif

	bottomTabs->addTab("Waveform", 	wave2DPair, 		cmdStr + "-r");
	bottomTabs->addTab("Effects", 	ev_bottomRight, 	cmdStr + "-f");
	bottomTabs->addTab("Presets", 	presetPageBounds, 	"p");
	bottomTabs->setSelectedTab(TabWaveform);
	bottomTabs->addListener(this);

	topTabs->addTab("Spectrum", 	spectrum2D->getZoomPanel());
	topTabs->addTab("Deformers", 	dfrmPair);
	topTabs->setSelectedTab(TabSpectrum);
	topTabs->addListener(this);

	addCorePanels();
	setMouseCursor(MouseCursor::CrosshairCursor);

	Bounded* temp[] = {
		wavePair, 	envPair,
		spectPair, 	keybBounds, 	consBounds,
		genBounds, 	propsBounds, 	modBounds,
		bnrBounds, 	oscCtrlBounds, 	playbackBounds,
		menuBounds,	wave2DPair, 	dfrmPair
	};

	for(int i = 0; i < numElementsInArray(temp); ++i)
		toOutline.insert(temp[i]);

	toOutline.insert(spectrum2D->getZoomPanel());
	toOutline.insert(wshpPair);
	toOutline.insert(irmodPair);
	toOutline.insert(unisonBounds);
	toOutline.insert(delayBounds);
	toOutline.insert(reverbBounds);
	toOutline.insert(eqBounds);

	initialized = true;
}


void MainPanel::initialiseMainView() {
    //								a					b				sbs		portion					name				border	min1	max1		min2	max2
	cv_genKeyBnr	 = new PanelPair(repo, genBounds,  	keybBounds, 	false, 	0.4f, 					"cv_keymod", 		mBord, 	60, 	INT_MAX, 	80, 	82	);
	cv_bnrOscCtrls	 = new PanelPair(repo, cv_genKeyBnr, oscCtrlBounds, true, 	0.85f, 					"cv_bnroscctrls", 	mBord, 	0, 		INT_MAX, 	90, 	105	);
	cv_rttb_pair	 = new PanelPair(repo, modBounds, 	cv_bnrOscCtrls, true, 	0.35f,  				"cv_rttb_pair",		mBord,  250, 	340						);
	cv_menuCons		 = new PanelPair(repo, menuBounds, 	consBounds,		true,	0.4f, 					"cv_menuCons", 		mBord, 	260, 	300,		200			);
	cv_rtt_pair		 = new PanelPair(repo, cv_menuCons, cv_rttb_pair,	false,	0.14f, 					"cv_menuGen",		mBord, 	24, 	25						);
	cv_middlePair	 = new PanelPair(repo, spectrum2D->getZoomPanel(),	cv_topTabBounds,true, 	0.98f, 	"cv_middlepair", 	sBord, 	0, 		INT_MAX, 	22, 	22	);
	cv_rtb_pair		 = new PanelPair(repo, propsBounds,	cv_middlePair,	true, 	0.22f,					"cv_rtb_pair",		mBord,	200, 	250						);
	cv_rt_pair		 = new PanelPair(repo, cv_rtt_pair,	cv_rtb_pair,	false,	0.3f,					"cv_rt_pair",		mBord,	200,	220						);
	cv_bottomPair	 = new PanelPair(repo, wave2DPair,	cv_botTabBounds,true, 	0.98f, 					"cv_bottompair", 	sBord, 	0, 		INT_MAX, 	22, 	22	);
	cv_right		 = new PanelPair(repo, cv_rt_pair,	cv_bottomPair,	false, 	cv_middlePortion, 		"cv_right",			mBord,  400,	INT_MAX, 	240			);
	cv_spectSurf 	 = new PanelPair(repo, spectPair, 	wavePair,		false, 	cv_spectSurfPortion,	"cv_f3dSurf", 		lBord,	190, 	INT_MAX, 	190			);
	cv_leftTriple 	 = new PanelPair(repo, cv_spectSurf, envPair,		false, 	cv_surfEnvPortion, 		"cv_leftTriple", 	lBord,	360, 	INT_MAX, 	150			);
	cv_playbackLeft	 = new PanelPair(repo, playbackBounds,cv_leftTriple,false, 	0.02f, 					"cv_playbackLeft", 	mBord,	24, 	INT_MAX, 	28			);
	cv_whole 		 = new PanelPair(repo, cv_playbackLeft,cv_right, 	true, 	cv_wholePortion, 		"cv_whole", 		lBord, 	250, 	800, 		650			);

	cv_spectSurf->setDragger(cv_spectSurfDragger);
	cv_leftTriple->setDragger(cv_envSpectDragger);

	cv_whole->setDragger(cv_wholeDragger);
	cv_right->setDragger(cv_middleDragger);
}


void MainPanel::initialiseExtendedView() {
    ZoomPanel* spectZoom = spectrum2D->getZoomPanel();

	xv_ctrl_key		= new PanelPair(repo, genBounds, 	keybBounds,		false,	0.4f, 					"xv_ctrl_key", 		mBord, 	60, 	INT_MAX, 	80, 	82	);
	xv_dfrm_imp		= new PanelPair(repo, dfrmPair, 	irmodPair, 		true, 	xv_dfrmImpPortion, 		"xv_dfrm_imp",		mmBord									);
	xv_envDfmImp	= new PanelPair(repo, envPair,		xv_dfrm_imp,	true, 	xv_envDfmImpPortion, 	"xv_brws_dfm_imp",	mmBord									);
	xv_TRTRBR_pair 	= new PanelPair(repo, xv_ctrl_key,	oscCtrlBounds,	true,	0.85f, 					"xv_trtrbr_pair", 	mBord, 	0, 		INT_MAX, 	100, 	105	);
	xv_TRTRB_pair 	= new PanelPair(repo, modBounds, 	xv_TRTRBR_pair, true,	0.4f,					"xv_trtrb_pair", 	mBord, 	210, 	370				 		);
	xv_TRTR_pair 	= new PanelPair(repo, cv_menuCons,	xv_TRTRB_pair,  false,	0.14f,					"xv_trtr_pair", 	mBord, 	24, 	25, 		110			);
	xv_FX_pair_1 	= new PanelPair(repo, unisonBounds, eqBounds, 		false, 	0.5f, 					"xv_unison_eq"												);
	xv_FX_Pair_2 	= new PanelPair(repo, reverbBounds, delayBounds, 	false, 	0.5f, 					"xv_reverb_dly"												);
	xv_FX_pair_A	= new PanelPair(repo, xv_FX_pair_1, xv_FX_Pair_2, 	false, 	0.5f, 					"xv_fx_pair_a"												);
	xv_TRBR_pair	= new PanelPair(repo, xv_FX_pair_A, wshpPair, 		false, 	0.5f, 					"xv_trbr_pair",		mmBord									);
	xv_TRBL_pair	= new PanelPair(repo, spectZoom,	wave2DPair,		false,	0.47f,					"xv_trbl_pair"												);
	xv_TRT_pair 	= new PanelPair(repo, propsBounds, 	xv_TRTR_pair, 	true, 	0.20f, 					"xv_trt_pair",  	mBord, 	180, 	220						);
	xv_TRB_pair 	= new PanelPair(repo, xv_TRBL_pair, xv_TRBR_pair, 	true, 	0.75f, 					"xv_trb_pair",		mBord, 	0, 		INT_MAX, 	255, 	450	);
	xv_TR_pair 		= new PanelPair(repo, xv_TRT_pair, 	xv_TRB_pair, 	false, 	0.25f, 					"xv_tr_pair",		mBord,  180, 	300						);
	xv_spectSurf	= new PanelPair(repo, spectPair,		wavePair,	false,	xv_spectSurfPortion,	"xv_spectSurf",		lBord									);
	xv_playbackLeft	= new PanelPair(repo, playbackBounds, xv_spectSurf,	false,	0.02f, 					"xv_playbackLeft", 	mBord,	24, 	INT_MAX, 	28			);
	xv_topPair 		= new PanelPair(repo, xv_playbackLeft,xv_TR_pair, 	true, 	xv_wholePortion, 		"xv_top_pair",		lBord, 	360, 	720						);
	xv_whole 		= new PanelPair(repo, xv_topPair, 	xv_envDfmImp, 	false, 	xv_topBttmPortion, 		"xv_whole", 		lBord, 	0, 		INT_MAX, 	165, 	350	);

	xv_spectSurfDragger	= new Dragger(repo, Dragger::ReduceF3dDetail  | Dragger::ReduceSurfDetail);
	xv_topBotDragger	= new Dragger(repo, Dragger::ReduceE3DDetail  | Dragger::ReduceF3dDetail | Dragger::ReduceSurfDetail);
	xv_wholeDragger 	= new Dragger(repo, Dragger::ReduceSurfDetail | Dragger::ReduceF3dDetail);
	xv_envDfmImpDragger	= new Dragger(repo, Dragger::ReduceNothing);
	xv_dfmImpDragger	= new Dragger(repo, Dragger::ReduceNothing);

	xv_dfrm_imp	->setDragger(xv_dfmImpDragger	);
	xv_topPair	->setDragger(xv_wholeDragger	);
	xv_envDfmImp->setDragger(xv_envDfmImpDragger);
	xv_whole	->setDragger(xv_topBotDragger	);
	xv_spectSurf->setDragger(xv_spectSurfDragger);
}


void MainPanel::initialiseFXView() {
    ev_reverbUni	= new PanelPair(repo, reverbBounds, unisonBounds,	true, 	0.47f,	"ev_reverbUni");
	ev_delayEQ 		= new PanelPair(repo, delayBounds,	eqBounds, 		true,	0.47f,	"ev_delayEQ"	);
	ev_paramFX		= new PanelPair(repo, ev_reverbUni,	ev_delayEQ,		false,	0.5f, 	"ev_paramfx"	);
	ev_tubeParamFX	= new PanelPair(repo, ev_paramFX, 	irmodPair, 		false,	0.3f, 	"ev_tubeparamfx", mBord, 104, 124);
	ev_bottomRight	= new PanelPair(repo, wshpPair, 		ev_tubeParamFX, true, 	0.35f,	"ev_bottomright");
}


void MainPanel::viewModeSwitched() {
    dout << "view mode switched to: " << viewMode << "\n";

	bool showingWaveform 	= bottomTabs->getSelectedId() == TabWaveform;
	bool showingDeformers 	= topTabs->getSelectedId() == TabDeformers;

    if (viewMode == CollapsedView) {
#ifndef BEAT_EDITION
        addAndMakeVisible(bottomTabs);
        addAndMakeVisible(topTabs);
#endif

        addAndMakeVisible(bannerPanel);

        toOutline.insert(bnrBounds);

        showingWaveform ?
        toggleEffectComponents(false) :
        toggleWaveform2DComponents(false);

        showingDeformers ?
        toggleF2D(false) :
        toggleDeformers(false);

        cv_middlePair->one = showingDeformers ? (Bounded*) dfrmPair : (Bounded*) spectrum2D->getZoomPanel();
        cv_middlePair->resized();

        toggleDraggers(true);
    } else {
#ifndef BEAT_EDITION
        removeChildComponent(topTabs);
        removeChildComponent(bottomTabs);
#endif

        removeChildComponent(bannerPanel);

        toOutline.erase(bnrBounds);

        showingWaveform ?
        toggleEffectComponents(true) :
        toggleWaveform2DComponents(true);

        showingDeformers ?
        toggleF2D(true) :
        toggleDeformers(true);

        toggleDraggers(false);
    }
}


void MainPanel::resized() {
    if (!initialized || getWidth() == 0)
        return;

	needsRepaint = true;

	int subtrahendW = 4, subtrahendH = 4, addand = 2;
	int width 	= getWidth() - subtrahendW;
	int height 	= getHeight() - subtrahendH;

	ViewMode oldView = viewMode;
	viewMode = height < 720 ? CollapsedView : UnifiedView;

#ifndef BEAT_EDITION
    if (oldView != viewMode)
        viewModeSwitched();
#endif

    Rectangle<int> wholeBounds(addand, addand, width, height);
    PanelPair* pair = viewMode == CollapsedView ? cv_whole : xv_whole;

    if (width != pair->getWidth() || height != pair->getHeight() || forceResize || oldView != viewMode) {
        {
            ScopedLock sl(getObj(MasterRenderer).getRenderLock());
            pair->setBounds(wholeBounds);
        }

        if (Util::assignAndWereDifferent(attachNextResize, false))
            attachVisibleComponents();

        stopTimer(BoundsCheckId);
        startTimer(BoundsCheckId, 200);
    }

    keyboard->setLowestVisibleKey(36);
}


void MainPanel::detachComponent(PanelGroup& group) {
    return;
//	GLParent* parent = group.gl;
//
//	if(parent != nullptr)
//	{
//		parent->detach();
//
//	  #ifdef SINGLE_OPENGL_THREAD
//		getObj(MasterRenderer).removeContext(parent, group.panel->getName());
//	  #endif
//	}
}


void MainPanel::attachComponent(PanelGroup& group) {
    OpenGLBase* parent = group.gl;
    if (parent != nullptr) {
        parent->attach();

	  #ifdef SINGLE_OPENGL_THREAD
		getObj(MasterRenderer).addContext(parent, group.panel->getName());
	  #endif
	}
}


void MainPanel::attachVisibleComponents() {
    dout << "Attaching visible components\n";

    for (int i = 0; i < panelGroups.size(); ++i) {
        PanelGroup* group = panelGroups[i];
        attachComponent(*group);
	}
}


void MainPanel::detachVisibleComponents() {
    return;
}


void MainPanel::paint(Graphics& g) {
    if (!gainedFocus && !hasKeyboardFocus(true)) {
        if (getScreenBounds().contains(Desktop::getMousePosition())) {
            grabKeyboardFocus();
            gainedFocus = true;
        }
    }

  #if PLUGIN_MODE
	g.fillAll(Colours::black);
  #endif

	g.setColour(Colour(150, 150, 180));

	for(set<Bounded*>::const_iterator it = toOutline.begin();
        it != toOutline.end(); ++it) {
        if ((*it)->getWidth() == 0)
            continue;

		float x = float((*it)->getX() - mBord / 3);
		float y = float((*it)->getY() - mBord / 3);
		float w = float((*it)->getWidth() + 2 * mBord / 3 - 1);
		float h = float((*it)->getHeight() + 2 * mBord / 3 - 1);

		Rectangle<int> rr(x, y, w, h);

		g.setColour(Colours::black);
		g.drawRect(x + 1.5f, y + 1.5f, w - 2, h - 2);

		g.setColour(Colour::greyLevel(0.2f));
		g.drawRect(x + 0.f, y + 0.f, w + 1.f, h + 1.f);

		g.setColour(Colour::greyLevel(0.1f));
		g.drawRect(x - 1, y - 1, w + 3, h + 3);
	}

    if (focusedPanel) {
        float x = float(focusedPanel->getX() - mBord / 3);
        float y = float(focusedPanel->getY() - mBord / 3);
		float w = float(focusedPanel->getWidth() + 2 * mBord / 3 - 1);
		float h = float(focusedPanel->getHeight() + 2 * mBord / 3 - 1);

		g.setColour(Colour(0.6f, 0.3f, 0.65f, 1.f));
		Rectangle<int> rr(x, y, w, h);
		getObj(MiscGraphics).drawCorneredRectangle(g, rr, 2);
	}

	needsRepaint = false;
}


bool MainPanel::keyPressed(const KeyPress& key) {
    return getObj(KeyboardInputHandler).keyPressed(key, this);
}


void MainPanel::toggleWaveform2DComponents(bool add) {
    if (add) {
        addAndMakeVisible(derivPanel);
        addAndMakeVisible(waveform2D->getZoomPanel());
    } else {
        removeChildComponent(waveform2D->getZoomPanel());
        removeChildComponent(derivPanel);
    }
}


void MainPanel::toggleEffectComponents(bool add) {
    if (add) {
        toOutline.insert(wshpPair);
		toOutline.insert(irmodPair);
		toOutline.insert(unisonBounds);
		toOutline.insert(delayBounds);
		toOutline.insert(reverbBounds);
		toOutline.insert(eqBounds);

		addAndMakeVisible(waveshaperUI->getZoomPanel());
		addAndMakeVisible(waveshaperUI->getControlsPanel());
		addAndMakeVisible(irModelUI->getZoomPanel());
		addAndMakeVisible(irModelUI->getControlsComponent());
		addAndMakeVisible(unisonUI);
		addAndMakeVisible(reverbUI);
		addAndMakeVisible(delayUI);
		addAndMakeVisible(eqUI);
    } else {
        removeChildComponent(waveshaperUI->getZoomPanel());
		removeChildComponent(waveshaperUI->getControlsPanel());
		removeChildComponent(irModelUI->getZoomPanel());
		removeChildComponent(irModelUI->getControlsComponent());
		removeChildComponent(unisonUI);
		removeChildComponent(reverbUI);
		removeChildComponent(eqUI);
		removeChildComponent(delayUI);

		toOutline.erase(wshpPair);
		toOutline.erase(irmodPair);
		toOutline.erase(unisonBounds);
		toOutline.erase(delayBounds);
		toOutline.erase(reverbBounds);
		toOutline.erase(eqBounds);
	}
}


void MainPanel::togglePresetPage(bool add) {
}


void MainPanel::toggleEffectsWaveform2DPresets(int toShow) {
    if (toShow == TabWaveform) {
        toggleEffectComponents(false);
		togglePresetPage(false);

		cv_bottomPair->one = wave2DPair;
		cv_bottomPair->resized();

		toggleWaveform2DComponents(true);
    } else if (toShow == TabEffects) {
        toggleWaveform2DComponents(false);
        togglePresetPage(false);

        cv_bottomPair->one = ev_bottomRight;
        cv_bottomPair->resized();

        toggleEffectComponents(true);
    } else if (toShow == TabPresets) {
        getObj(Dialogs).showPresetBrowserModal();
    }

	focusedPanel = 0;
	needsRepaint = true;

	repaint();
}


void MainPanel::toggleDeformers(bool add) {
    if (add) {
        addAndMakeVisible(dfrmPanel->getZoomPanel());
        addAndMakeVisible(dfrmPanel->getControlsComponent());

        noBeat(toOutline.insert(dfrmPair));
    } else {
        removeChildComponent(dfrmPanel->getZoomPanel());
        removeChildComponent(dfrmPanel->getControlsComponent());

        noBeat(toOutline.erase(dfrmPair));
    }
}

void MainPanel::toggleF2D(bool add) {
    if (add) {
        addAndMakeVisible(spectrum2D->getZoomPanel());
        toOutline.insert(spectrum2D->getZoomPanel());
    } else {
        removeChildComponent(spectrum2D->getZoomPanel());
        toOutline.erase(spectrum2D->getZoomPanel());
    }
}


void MainPanel::toggleF2DGuide(bool wantToShowDeformers) {
    if (wantToShowDeformers) {
        toggleF2D(false);

		cv_middlePair->one = dfrmPair;
		cv_middlePair->resized();

		toggleDeformers(true);
    } else {
        toggleDeformers(false);

		cv_middlePair->one = spectrum2D->getZoomPanel();
		cv_middlePair->resized();

		toggleF2D(true);
	}

	focusedPanel = 0;
	needsRepaint = true;

	repaint();
}


void MainPanel::toggleEnvPanel(bool wantToShow3D) {
    if (wantToShow3D) {
        removeChildComponent(envelope2D->getZoomPanel());

		envPair->two = envelope3D->getZoomPanel();
		envPair->resized();

        addAndMakeVisible(envelope3D->getZoomPanel());
    } else {
        removeChildComponent(envelope3D->getZoomPanel());

		envPair->two = envelope2D->getZoomPanel();
		envPair->resized();

		addAndMakeVisible(envelope2D->getZoomPanel());
	}

	envelopeVisibilityChanged();

	envelope2D->updateBackground(false);
	focusedPanel = 0;
	needsRepaint = true;

	repaint();
}


void MainPanel::mouseEnter(const MouseEvent& e) {
}


void MainPanel::mouseDown(const MouseEvent& e) {
    grabKeyboardFocus();

    Component* origin = e.eventComponent;
	Bounded* oldFocusComp = focusedPanel;

    for (int i = 0; i < panelGroups.size(); ++i) {
        PanelGroup* group = panelGroups[i];

        if (group == nullptr)
            continue;

        if (group->panel->getComponent() == origin) {
            focusedPanel = group->bounds;
            getObj(VertexPropertiesPanel).setSelectedAndCaller(group->panel->getInteractor());
		}
	}

    if (focusedPanel != oldFocusComp) {
        needsRepaint = true;
        repaint();
    }
}


Bounded* MainPanel::getFocusedComponent() {
    return focusedPanel;
}


void MainPanel::removeListeners() {
    if (menuBar != nullptr)
        menuBar->setModel(0);
}


void MainPanel::childrenChanged() {
    needsRepaint = true;
}


void MainPanel::childBoundsChanged(Component* child) {
    needsRepaint = true;
}

void MainPanel::addCorePanels() {
    Component* spectCtrls  	= spectrum3D->getControlsComponent();
	Component* surfCtrls 	= waveform3D->getControlsComponent();
	Component* envCtrls  	= envelope2D->getControlsComponent();

	addAndMakeVisible(derivPanel);
	addAndMakeVisible(spectrum3D->getZoomPanel());
	addAndMakeVisible(spectrum2D->getZoomPanel());

	if(getSetting(CurrentMorphAxis) == Vertex::Time)
		addAndMakeVisible(envelope2D->getZoomPanel());
	else
		addAndMakeVisible(envelope3D->getZoomPanel());

	addAndMakeVisible(playbackPanel);
	addAndMakeVisible(waveform2D->getZoomPanel());
	addAndMakeVisible(waveform3D->getZoomPanel());

	addAndMakeVisible(console);
	addAndMakeVisible(menuBar);
	addAndMakeVisible(morphPanel);
	addAndMakeVisible(vtxPropsPanel);
	addAndMakeVisible(keyboard);
	addAndMakeVisible(generalControls);

	addAndMakeVisible(spectCtrls);
	addAndMakeVisible(surfCtrls);
	addAndMakeVisible(envCtrls);

	toggleDraggers(false);

	addAndMakeVisible(&getObj(OscControlPanel));
	addAndMakeVisible(waveshaperUI->getControlsPanel());
	addAndMakeVisible(irModelUI->getControlsComponent());
	addAndMakeVisible(dfrmPanel->getControlsComponent());
	addAndMakeVisible(dfrmPanel->getZoomPanel());
	addAndMakeVisible(irModelUI->getZoomPanel());
	addAndMakeVisible(waveshaperUI->getZoomPanel());
	addAndMakeVisible(unisonUI);
	addAndMakeVisible(reverbUI);
	addAndMakeVisible(delayUI);
	addAndMakeVisible(eqUI);
	addAndMakeVisible(presetPage);
}

void MainPanel::setPlayerComponents() {
    addAndMakeVisible(console);
    addAndMakeVisible(keyboard);
}

void MainPanel::triggerTabClick(int tab) {
    bottomTabs->selectTab(tab);
}

void MainPanel::toggleDraggers(bool showCollapsed) {
    if (showCollapsed) {
        removeChildComponent(xv_wholeDragger);
		removeChildComponent(xv_topBotDragger);
		removeChildComponent(xv_spectSurfDragger);
		removeChildComponent(xv_dfmImpDragger);
		removeChildComponent(xv_envDfmImpDragger);

		addAndMakeVisible(cv_wholeDragger);
		addAndMakeVisible(cv_middleDragger);
        addAndMakeVisible(cv_envSpectDragger);
        addAndMakeVisible(cv_spectSurfDragger);
    } else {
        removeChildComponent(cv_wholeDragger);
		removeChildComponent(cv_middleDragger);
		removeChildComponent(cv_envSpectDragger);
		removeChildComponent(cv_spectSurfDragger);

		addAndMakeVisible(xv_wholeDragger);
		addAndMakeVisible(xv_topBotDragger);
		addAndMakeVisible(xv_spectSurfDragger);
		addAndMakeVisible(xv_dfmImpDragger);
		addAndMakeVisible(xv_envDfmImpDragger);
	}
}


void MainPanel::switchedRenderingMode(bool shouldDoUpdate) {
    attachNextResize = true;

//	jassert(crsGL == 0);

	// needs to be resized first
//	crsGL 	= new OpenGLPanel	(repo, waveform2D);
//	f2GL 	= new OpenGLPanel	(repo, spectrum2D);
//	dfmGL	= new OpenGLPanel	(repo, dfrmPanel);
//	e2GL 	= new OpenGLPanel	(repo, envelope2D);
//	wsGL 	= new OpenGLPanel	(repo, waveshaperUI);
//	tmGL 	= new OpenGLPanel	(repo, irModelUI);
//	f3GL	= new OglSpectrum3D (repo, spectrum3D);
//	e3GL	= new OglEnvelope3D	(repo, envelope3D);
//	surfGL 	= new OglWaveform3D	(repo, waveform3D);

//	wave2DZoomPanel	->panelComponentChanged(crsGL);
//	spectZoomPanel	->panelComponentChanged(f3GL);
//	envZoomPanel	->panelComponentChanged(e2GL);
//	e3dZoomPanel	->panelComponentChanged(e3GL);
//	f2dZoomPanel	->panelComponentChanged(f2GL);
//	wave3DZoomPanel	->panelComponentChanged(surfGL);
//	tubeZoomPanel	->panelComponentChanged(tmGL);
//	wsZoomPanel		->panelComponentChanged(wsGL);
//	dfrmZoomPanel	->panelComponentChanged(dfmGL);

	wave2DGroup = PanelGroup(waveform2D, 	wave2DPair, waveform2D->getOpenglPanel());
	surfGroup 	= PanelGroup(waveform3D, 	wavePair, 	waveform3D->getOpenglPanel());
	spectGroup3	= PanelGroup(spectrum3D, 	spectPair, 	spectrum3D->getOpenglPanel());
	envGroup2	= PanelGroup(envelope2D, 	envPair, 	envelope2D->getOpenglPanel());
	wshpGroup	= PanelGroup(waveshaperUI,	wshpPair, 	waveshaperUI->getOpenglPanel());
	irGroup		= PanelGroup(irModelUI,		irmodPair, 	irModelUI->getOpenglPanel());
	dfrmGroup	= PanelGroup(dfrmPanel,		dfrmPair, 	dfrmPanel->getOpenglPanel());
	spectGroup2	= PanelGroup(spectrum2D, 	spectrum2D->getZoomPanel(), spectrum2D->getOpenglPanel());
	envGroup3	= PanelGroup(envelope3D, 	envelope3D->getZoomPanel(), envelope3D->getOpenglPanel());

	attachVisibleComponents();

	spectrum3D->setUseVertices(true);
	waveform3D->setUseVertices(true);
	envelope3D->setUseVertices(true);

	spectrum3D->bakeTexturesNextRepaint();
	waveform3D->bakeTexturesNextRepaint();
	envelope3D->bakeTexturesNextRepaint();

	panelComponentsChanged();

    if (shouldDoUpdate) {
        ScopedBooleanSwitcher sbs(forceResize);

		resized();
		getObj(Updater).update(UpdateSources::SourceAll, (int) UpdateType::Repaint);

		repaint();
	}
}


void MainPanel::componentVisibilityChanged() {
    dout << "Main component visibility changed: " << (isShowing() ? "showing" : "not showing") << "\n";

//	if(isVisible())
//	{
//		if(Util::assignAndWereDifferent(attachNextResize, false))
//		{
//			attachVisibleComponents();
//		}
//		else
//		{
		// on osx this seems to be a race condition with context addition and window repainting
//	      #ifdef SINGLE_OPENGL_THREAD
//			getObj(MasterRenderer).repaintAll();
//		  #else
//			if(getSetting(UseOpenGL))
//				repaintAll();
//	      #endif
//		}
//	}
}


void MainPanel::repaintAll() {
    for (int i = 0; i < panelGroups.size(); ++i) {
        PanelGroup* group = panelGroups[i];

        if (Component * c = group->panel->getComponent()) {
            if (c->isVisible())
				c->repaint();
		}
	}
}

void MainPanel::componentPeerChanged() {
    dout << "Main Panel component peer changed\n";
}


void MainPanel::focusGained(FocusChangeType type) {
    dout << "Main panel focus gained: " << (int) type << "\n";
}


void MainPanel::componentMovedOrResized(bool wasMoved, bool wasResized) {
    dout << "Main panel moved or resized, repainting all\n";

#ifdef SINGLE_OPENGL_THREAD
    if(isVisible())
        getObj(MasterRenderer).repaintAll();
#else
    repaintAll();
#endif
}


void MainPanel::writeXML(XmlElement* element) const {
    XmlElement* mainXml = new XmlElement("MainPanel");

	mainXml->setAttribute("CV_WholeDragger", 	(double) cv_wholeDragger	->getPair()->getPortion());
	mainXml->setAttribute("CV_MiddleDragger", 	(double) cv_middleDragger	->getPair()->getPortion());
	mainXml->setAttribute("CV_EnvSpectDragger", (double) cv_envSpectDragger	->getPair()->getPortion());
	mainXml->setAttribute("CV_SpectSurfDragger",(double) cv_spectSurfDragger->getPair()->getPortion());

	mainXml->setAttribute("XV_WholeDragger", 	(double) xv_wholeDragger	->getPair()->getPortion());
	mainXml->setAttribute("XV_TopBttmDragger", 	(double) xv_topBotDragger	->getPair()->getPortion());
	mainXml->setAttribute("XV_SpectSurfDragger",(double) xv_spectSurfDragger->getPair()->getPortion());
	mainXml->setAttribute("XV_EnvDfmImpDragger",(double) xv_envDfmImpDragger->getPair()->getPortion());
	mainXml->setAttribute("XV_DfrmImpDragger", 	(double) xv_dfmImpDragger	->getPair()->getPortion());

	element->addChildElement(mainXml);
}


bool MainPanel::readXML(const XmlElement* element) {
    if (element == nullptr)
        return false;

	XmlElement* mainXml = element->getChildByName("MainPanel");

	if(mainXml == nullptr)
		return false;

	cv_wholePortion 	= mainXml->getDoubleAttribute("CV_WholeDragger", 		cv_wholePortion);
	cv_middlePortion 	= mainXml->getDoubleAttribute("CV_MiddleDragger", 		cv_middlePortion);
	cv_spectSurfPortion = mainXml->getDoubleAttribute("CV_SpectSurfDragger",	cv_spectSurfPortion);
	cv_surfEnvPortion	= mainXml->getDoubleAttribute("CV_SurfEnvDragger",  	cv_surfEnvPortion);

	xv_wholePortion 	= mainXml->getDoubleAttribute("XV_WholeDragger", 		xv_wholePortion);
	xv_topBttmPortion 	= mainXml->getDoubleAttribute("XV_TopBttmDragger", 		xv_topBttmPortion);
	xv_spectSurfPortion = mainXml->getDoubleAttribute("XV_SpectSurfDragger",	xv_spectSurfPortion);
	xv_dfrmImpPortion	= mainXml->getDoubleAttribute("XV_DfrmImpDragger",  	xv_dfrmImpPortion);
	xv_envDfmImpPortion	= mainXml->getDoubleAttribute("XV_EnvDfmImpDragger", 	xv_envDfmImpPortion);

	return true;
}


void MainPanel::timerCallback(int timerId) {
    if (timerId == BoundsCheckId) {
		Panel* panels[] =
		{
			spectrum2D, spectrum3D, waveform3D, waveform2D, dfrmPanel,
			irModelUI, waveshaperUI, envelope2D, envelope3D
		};

		const Desktop& desktop = Desktop::getInstance();

		for(int panelIdx = 0; panelIdx < numElementsInArray(panels); ++panelIdx) {
            Panel* panel = panels[panelIdx];
            if (Component * c = panel->getComponent()) {
                if (c->isShowing()) {
                    Rectangle<int> bounds = c->getScreenBounds().reduced(5, 5);

                    if (bounds.isEmpty())
                        continue;

					Point<int> points[] = { bounds.getTopLeft(),
											bounds.getBottomLeft(),
											bounds.getTopRight(),
											bounds.getBottomRight() };

					int numCornersOverlapped = 0;
                    for (int pointIdx = 0; pointIdx < numElementsInArray(points); ++pointIdx) {
                        if (desktop.findComponentAt(points[pointIdx]) != c) {
                            ++numCornersOverlapped;
                        }
                    }

                    if (numCornersOverlapped != panel->getNumCornersOverlapped()) {
                        panel->repaint();
                        panel->setNumCornersOverlapped(numCornersOverlapped);
                    }
				}
			}
		}

		stopTimer(BoundsCheckId);
    } else if (timerId == DelayedRepaint) {
#ifdef JUCE_MAC
        doUpdate(SourceMorph);
#endif

        repaintAll();
        stopTimer(DelayedRepaint);
	}
}


void MainPanel::resetTabToWaveform() {
    bottomTabs->setSelectedTab(TabWaveform);
}


void MainPanel::triggerDelayedRepaint() {
    startTimer(DelayedRepaint, 200);
}


Component* MainPanel::getComponent(int which) {
    switch (which) {
        case CompWaveform2DZoomH:
            return waveform2D->getZoomPanel()->getComponent(true);
            break;
        case CompWaveform2DZoomW:
            return waveform2D->getZoomPanel()->getComponent(false);
            break;
    }

    return nullptr;
}


void MainPanel::setPrimaryDimension(int view, bool performUpdate) {}


void MainPanel::envelopeVisibilityChanged() {
    // XXX
}


void MainPanel::panelComponentsChanged() {

}

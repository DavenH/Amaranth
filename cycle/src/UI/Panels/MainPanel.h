#ifndef _mainpanel_h
#define _mainpanel_h

#include <vector>
#include <set>
#include <App/Doc/Savable.h>
#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/Widgets/TabbedSelector.h>
#include "../App/WaveDragTarget.h"

using std::vector;
using std::set;

class AudioSourceRepo;
class BannerPanel;
class Bounded;
class ConfigPanel;
class Console;
class DeformerPanel;
class DerivativePanel;
class Dragger;
class EffectPanel;
class Envelope2D;
class Envelope3D;
class GeneralControls;
class KeyboardInputHandler;
class MidiKeyboard;
class MorphPanel;
class OpenGLBase;
class Panel;
class PanelPair;
class PlaybackPanel;
class PresetPage;
class PropertiesPanel;
class Spectrum2D;
class Spectrum3D;
class VertexPropertiesPanel;
class Waveform2D;
class Waveform3D;
class ZoomPanel;

class DelayUI;
class EqualizerUI;
class IrModellerUI;
class OpenGLPanel;
class OpenGLPanel3D;
class PhaserUI;
class ReverbUI;
class UnisonUI;
class WaveshaperUI;

class PanelGroup {
public:
	Panel* 		panel;
	Bounded* 	bounds;
	OpenGLBase* gl;

	PanelGroup() : panel(nullptr), bounds(nullptr), gl(nullptr) {}
	PanelGroup(Panel* p, Bounded* bo, OpenGLBase* gl) : panel(p), bounds(bo), gl(gl){}
};

class MainPanel :
		public Component
	,	public TabbedSelector::Listener
	,	public ComponentMovementWatcher
	,	public WaveDragTarget
	, 	public MultiTimer
	,	public Savable {
public:
	enum Borders { zBord = 0, xsBord = 1, sBord = 2, mBord = 6, mmBord = 7, lBord = 9 };

	enum ViewMode {
        UnifiedView,
        CollapsedView,

        NumViewModes
    };

    enum BottomTabs {
        TabWaveform,
        TabEffects,
        TabPresets
    };

    enum TopTabs {
        TabSpectrum,
        TabDeformers,
    };

    enum TimerIds {
        BoundsCheckId,
        DelayedRepaint
    };

    enum {
        CompWaveform2DZoomH,
        CompWaveform2DZoomW
    };

    MainPanel(SingletonRepo* repo);
	virtual ~MainPanel();

	void init();
	void resized();

	void paint(Graphics& g);
	bool keyPressed(const KeyPress& key);
	void tabSelected(TabbedSelector* selector, Bounded* callbackComponent);

	void initialisePanels();
	void initialiseExtendedView();
	void initialiseMainView();
	void initialiseFXView();

	void toggleWaveform2DComponents(bool add);
	void toggleEffectComponents(bool add);
	void togglePresetPage(bool add);
	void toggleEffectsWaveform2DPresets(int toShow);
	void toggleDeformers(bool add);
	void toggleF2D(bool add);
	void toggleF2DGuide(bool wantToShowGuide);
	void toggleEnvPanel(bool wantToShow3D);
	void toggleDraggers(bool showCollapsed);

	void triggerTabClick(int tab);
	void resetTabToWaveform();

	Bounded* getFocusedComponent();
	void addCorePanels();
	void attachVisibleComponents();
	void childBoundsChanged (Component *child);
	void childrenChanged();
	void componentMovedOrResized (bool wasMoved, bool wasResized);
    void componentPeerChanged();
    void componentVisibilityChanged();
	void detachVisibleComponents();

	void envelopeVisibilityChanged();
	void panelComponentsChanged();

    void focusGained(FocusChangeType type);
	void mouseDown(const MouseEvent& e);
	void mouseEnter(const MouseEvent& e);
	void removeListeners();
	void repaintAll();
	void setAttachNextResize(bool should) { attachNextResize = should; }
	void setPlayerComponents();
    void setPrimaryDimension(int view, bool performUpdate);
	void switchedRenderingMode(bool doUpdate);
    void timerCallback(int timerId);
    void triggerDelayedRepaint();

	Component* getComponent(int which);
	bool& getForceResizeFlag() { return forceResize; }

	void writeXML(XmlElement* element) const;
	bool readXML(const XmlElement* element);

private:
	void attachComponent(PanelGroup& group);
	void detachComponent(PanelGroup& group);
	void viewModeSwitched();

    class ItrFocus {
    public:
		ItrFocus(Panel* c, Bounded* b) : source(c), bounds(b) {}
		Panel* source;
		Bounded* bounds;
	};

	ScopedPointer<Bounded> presetPageBounds;
//	ScopedPointer<PanelPair> wave2DTabs;

	// these fuckers always stick together
	ScopedPointer<PanelPair> wavePair;
	ScopedPointer<PanelPair> spectPair;
	ScopedPointer<PanelPair> envPair;

	ScopedPointer<PanelPair> ev_bottomRight;
	ScopedPointer<PanelPair> ev_reverbUni;
	ScopedPointer<PanelPair> ev_delayEQ;
	ScopedPointer<PanelPair> ev_paramFX;
	ScopedPointer<PanelPair> ev_tubeParamFX;
	ScopedPointer<PanelPair> cv_bottomPair;
	ScopedPointer<PanelPair> cv_middlePair;

	ScopedPointer<PanelPair> wave2DPair;
	ScopedPointer<PanelPair> dfrmPair;
	ScopedPointer<PanelPair> irmodPair;
	ScopedPointer<PanelPair> wshpPair;

	// main view mode
	ScopedPointer<PanelPair> cv_bnrOscCtrls;
	ScopedPointer<PanelPair> cv_genKeyBnr;
	ScopedPointer<PanelPair> cv_leftTriple;
//	ScopedPointer<PanelPair> cv_keyBnr;
//	ScopedPointer<PanelPair> cv_f2dKeyGen;
//	ScopedPointer<PanelPair> cv_topRight;
//	ScopedPointer<PanelPair> cv_propsMod;
	ScopedPointer<PanelPair> cv_menuCons;
	ScopedPointer<PanelPair> cv_playbackLeft;
	ScopedPointer<PanelPair> cv_right;
	ScopedPointer<PanelPair> cv_rt_pair;
	ScopedPointer<PanelPair> cv_rtb_pair;
	ScopedPointer<PanelPair> cv_rtt_pair;
	ScopedPointer<PanelPair> cv_rttb_pair;
	ScopedPointer<PanelPair> cv_spectSurf;
	ScopedPointer<PanelPair> cv_whole;

	ScopedPointer<PanelPair> xv_ctrl_key;
	ScopedPointer<PanelPair> xv_envDfmImp;
	ScopedPointer<PanelPair> xv_dfrm_imp;
	ScopedPointer<PanelPair> xv_FX_pair_1;
	ScopedPointer<PanelPair> xv_FX_Pair_2;
	ScopedPointer<PanelPair> xv_FX_pair_A;
	ScopedPointer<PanelPair> xv_TRTRBR_pair;
	ScopedPointer<PanelPair> xv_TRTRB_pair;
	ScopedPointer<PanelPair> xv_TRTR_pair;
	ScopedPointer<PanelPair> xv_TRBR_pair;
	ScopedPointer<PanelPair> xv_TRBL_pair;
	ScopedPointer<PanelPair> xv_TRT_pair;
	ScopedPointer<PanelPair> xv_TRB_pair;
	ScopedPointer<PanelPair> xv_TR_pair;
	ScopedPointer<PanelPair> xv_topPair;
	ScopedPointer<PanelPair> xv_whole;
	ScopedPointer<PanelPair> xv_spectSurf;
	ScopedPointer<PanelPair> xv_playbackLeft;

//	ScopedPointer<PanelPair> bv_dfrm_imp;
//	ScopedPointer<PanelPair> bv_envDfmImp;
//	ScopedPointer<PanelPair> bv_FX_pair_1;
//	ScopedPointer<PanelPair> bv_FX_Pair_2;
//	ScopedPointer<PanelPair> bv_FX_pair_A;
//	ScopedPointer<PanelPair> bv_FX_pair;
//	ScopedPointer<PanelPair> bv_gen_key;
//	ScopedPointer<PanelPair> bv_left;
//	ScopedPointer<PanelPair> bv_playbackLeft;
//	ScopedPointer<PanelPair> bv_props_bnr;
//	ScopedPointer<PanelPair> bv_right;
//	ScopedPointer<PanelPair> bv_RT_pair;
//	ScopedPointer<PanelPair> bv_RTL_pair;
//	ScopedPointer<PanelPair> bv_RTLT_pair;
//	ScopedPointer<PanelPair> bv_RTLTB_pair;
//	ScopedPointer<PanelPair> bv_RTLTBL_pair;
//	ScopedPointer<PanelPair> bv_spectSurf;
//	ScopedPointer<PanelPair> bv_topPair;
//	ScopedPointer<PanelPair> bv_whole;

	ScopedPointer<Bounded> envCtrlBounds;

	ScopedPointer<TabbedSelector> topRightTabs;
	ScopedPointer<TabbedSelector> bottomTabs;
	ScopedPointer<TabbedSelector> topTabs;

	ScopedPointer<BannerPanel> bannerPanel;

	ScopedPointer<Bounded> keybBounds;
	ScopedPointer<Bounded> genBounds;
	ScopedPointer<Bounded> consBounds;
	ScopedPointer<Bounded> modBounds;
	ScopedPointer<Bounded> playbackBounds;

	ScopedPointer<Bounded> spectCtrlBounds;
	ScopedPointer<Bounded> surfCtrlBounds;
	ScopedPointer<Bounded> guideCtrlBounds;
	ScopedPointer<Bounded> tubeCtrlBoundsB;

//	ScopedPointer<Bounded> tubeCtrlBounds;
//	ScopedPointer<Bounded> envCtrlABounds;
//	ScopedPointer<Bounded> envCtrlBBounds;

//	ScopedPointer<ZoomPanel> spectZoomPanel;
//	ScopedPointer<ZoomPanel> e3dZoomPanel;
//	ScopedPointer<ZoomPanel> envZoomPanel;
//	ScopedPointer<ZoomPanel> f2dZoomPanel;
//	ScopedPointer<ZoomPanel> wave3DZoomPanel;
//	ScopedPointer<ZoomPanel> wave2DZoomPanel;
//	ScopedPointer<ZoomPanel> tubeZoomPanel;
//	ScopedPointer<ZoomPanel> wsZoomPanel;
//	ScopedPointer<ZoomPanel> dfrmZoomPanel;

	ScopedPointer<Bounded> derivBounds;
	ScopedPointer<Bounded> propsBounds;
	ScopedPointer<Bounded> unisonBounds;
	ScopedPointer<Bounded> bnrBounds;
	ScopedPointer<Bounded> menuBounds;
	ScopedPointer<Bounded> oscCtrlBounds;
//	ScopedPointer<Bounded> bottomCtrlBounds;
	ScopedPointer<Bounded> reverbBounds;
	ScopedPointer<Bounded> delayBounds;
	ScopedPointer<Bounded> eqBounds;
	ScopedPointer<Bounded> wsCtrlBounds;
//	ScopedPointer<Bounded> wave2DCtrlBounds;
	ScopedPointer<Bounded> cv_botTabBounds;
	ScopedPointer<Bounded> cv_topTabBounds;

	// singleton references
	Ref<VertexPropertiesPanel> vtxPropsPanel;
	Ref<GeneralControls> 	generalControls;
	Ref<Console> 			console;
	Ref<MorphPanel>			morphPanel;
	Ref<PlaybackPanel>		playbackPanel;
	Ref<Waveform3D> 		waveform3D;
	Ref<Waveform2D> 		waveform2D;
	Ref<PresetPage> 		presetPage;
	Ref<Spectrum2D> 		spectrum2D;
	Ref<Spectrum3D> 		spectrum3D;
	Ref<Envelope2D> 		envelope2D;
	Ref<Envelope3D> 		envelope3D;
//	Ref<KeyboardInputHandler> inputHandler;
//	Ref<AudioSourceRepo> 	audioManager;
	Ref<DerivativePanel> 	derivPanel;
	Ref<DeformerPanel> 		dfrmPanel;

	Ref<IrModellerUI> 	irModelUI;
	Ref<WaveshaperUI> 	waveshaperUI;
	Ref<UnisonUI> 		unisonUI;
	Ref<ReverbUI> 		reverbUI;
	Ref<DelayUI> 		delayUI;
	Ref<EqualizerUI> 	eqUI;
	Ref<MidiKeyboard> 	keyboard;

	ScopedPointer<MenuBarComponent> menuBar;

	ScopedPointer<Dragger> xv_topBotDragger;
	ScopedPointer<Dragger> xv_spectSurfDragger;
	ScopedPointer<Dragger> xv_envDfmImpDragger;
	ScopedPointer<Dragger> xv_dfmImpDragger;
	ScopedPointer<Dragger> xv_wholeDragger;

	ScopedPointer<Dragger> cv_wholeDragger;
	ScopedPointer<Dragger> cv_middleDragger;

	ScopedPointer<Dragger> cv_envSpectDragger;
	ScopedPointer<Dragger> cv_spectSurfDragger;

//	ScopedPointer<OpenGLPanel> crsGL;
//	ScopedPointer<OpenGLPanel> f2GL;
//	ScopedPointer<OpenGLPanel> e2GL;
//	ScopedPointer<OpenGLPanel> dfmGL;
//	ScopedPointer<OpenGLPanel> wsGL;
//	ScopedPointer<OpenGLPanel> tmGL;
//
//	ScopedPointer<OpenGLPanel3D> surfGL;
//	ScopedPointer<OpenGLPanel3D> f3GL;
//	ScopedPointer<OpenGLPanel3D> e3GL;

	PanelGroup wave2DGroup, surfGroup, spectGroup2, spectGroup3, envGroup2, envGroup3, wshpGroup, irGroup, dfrmGroup;
	Array<PanelGroup*> panelGroups;

	set<Bounded*> toOutline;

	bool initialized, gainedFocus, needsRepaint, forceResize, attachNextResize;
	float f2dEffectsWidth;
	float cv_wholePortion, cv_middlePortion, cv_surfEnvPortion, cv_spectSurfPortion;
	float xv_wholePortion, xv_spectSurfPortion, xv_dfrmImpPortion, xv_topBttmPortion, xv_envDfmImpPortion;

	ViewMode viewMode;
	Image bufferedImage;
	Bounded* focusedPanel;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel);
};

#endif

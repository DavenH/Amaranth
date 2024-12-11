#pragma once
#include <vector>
#include <set>
#include <App/Doc/Savable.h>
#include <App/SingletonAccessor.h>
#include <App/WaveDragTarget.h>
#include <Obj/Ref.h>
#include <UI/Widgets/TabbedSelector.h>

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

    explicit MainPanel(SingletonRepo* repo);
	~MainPanel() override;

	void init() override;
	void resized() override;

	void paint(Graphics& g) override;
	bool keyPressed(const KeyPress& key) override;
	void tabSelected(TabbedSelector* selector, Bounded* callbackComponent) override;

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
	void childBoundsChanged (Component *child) override;
	void childrenChanged() override;
	void componentMovedOrResized (bool wasMoved, bool wasResized) override;
    void componentPeerChanged() override;
    void componentVisibilityChanged() override;
	void detachVisibleComponents();

	void envelopeVisibilityChanged();
	void panelComponentsChanged();

    void focusGained(FocusChangeType type) override;
	void mouseDown(const MouseEvent& e) override;
	void mouseEnter(const MouseEvent& e) override;
	void removeListeners();
	void repaintAll();
	void setAttachNextResize(bool should) { attachNextResize = should; }
	void setPlayerComponents();
    void setPrimaryDimension(int view, bool performUpdate);
	void switchedRenderingMode(bool doUpdate);
    void timerCallback(int timerId) override;
    void triggerDelayedRepaint();

	Component* getComponent(int which);
	bool& getForceResizeFlag() { return forceResize; }

	void writeXML(XmlElement* element) const override;
	bool readXML(const XmlElement* element) override;

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

	std::unique_ptr<Bounded> presetPageBounds;
//	std::unique_ptr<PanelPair> wave2DTabs;

	// these fuckers always stick together
	std::unique_ptr<PanelPair> wavePair;
	std::unique_ptr<PanelPair> spectPair;
	std::unique_ptr<PanelPair> envPair;

	std::unique_ptr<PanelPair> ev_bottomRight;
	std::unique_ptr<PanelPair> ev_reverbUni;
	std::unique_ptr<PanelPair> ev_delayEQ;
	std::unique_ptr<PanelPair> ev_paramFX;
	std::unique_ptr<PanelPair> ev_tubeParamFX;
	std::unique_ptr<PanelPair> cv_bottomPair;
	std::unique_ptr<PanelPair> cv_middlePair;

	std::unique_ptr<PanelPair> wave2DPair;
	std::unique_ptr<PanelPair> dfrmPair;
	std::unique_ptr<PanelPair> irmodPair;
	std::unique_ptr<PanelPair> wshpPair;

	// main view mode
	std::unique_ptr<PanelPair> cv_bnrOscCtrls;
	std::unique_ptr<PanelPair> cv_genKeyBnr;
	std::unique_ptr<PanelPair> cv_leftTriple;
//	std::unique_ptr<PanelPair> cv_keyBnr;
//	std::unique_ptr<PanelPair> cv_f2dKeyGen;
//	std::unique_ptr<PanelPair> cv_topRight;
//	std::unique_ptr<PanelPair> cv_propsMod;
	std::unique_ptr<PanelPair> cv_menuCons;
	std::unique_ptr<PanelPair> cv_playbackLeft;
	std::unique_ptr<PanelPair> cv_right;
	std::unique_ptr<PanelPair> cv_rt_pair;
	std::unique_ptr<PanelPair> cv_rtb_pair;
	std::unique_ptr<PanelPair> cv_rtt_pair;
	std::unique_ptr<PanelPair> cv_rttb_pair;
	std::unique_ptr<PanelPair> cv_spectSurf;
	std::unique_ptr<PanelPair> cv_whole;

	std::unique_ptr<PanelPair> xv_ctrl_key;
	std::unique_ptr<PanelPair> xv_envDfmImp;
	std::unique_ptr<PanelPair> xv_dfrm_imp;
	std::unique_ptr<PanelPair> xv_FX_pair_1;
	std::unique_ptr<PanelPair> xv_FX_Pair_2;
	std::unique_ptr<PanelPair> xv_FX_pair_A;
	std::unique_ptr<PanelPair> xv_TRTRBR_pair;
	std::unique_ptr<PanelPair> xv_TRTRB_pair;
	std::unique_ptr<PanelPair> xv_TRTR_pair;
	std::unique_ptr<PanelPair> xv_TRBR_pair;
	std::unique_ptr<PanelPair> xv_TRBL_pair;
	std::unique_ptr<PanelPair> xv_TRT_pair;
	std::unique_ptr<PanelPair> xv_TRB_pair;
	std::unique_ptr<PanelPair> xv_TR_pair;
	std::unique_ptr<PanelPair> xv_topPair;
	std::unique_ptr<PanelPair> xv_whole;
	std::unique_ptr<PanelPair> xv_spectSurf;
	std::unique_ptr<PanelPair> xv_playbackLeft;

//	std::unique_ptr<PanelPair> bv_dfrm_imp;
//	std::unique_ptr<PanelPair> bv_envDfmImp;
//	std::unique_ptr<PanelPair> bv_FX_pair_1;
//	std::unique_ptr<PanelPair> bv_FX_Pair_2;
//	std::unique_ptr<PanelPair> bv_FX_pair_A;
//	std::unique_ptr<PanelPair> bv_FX_pair;
//	std::unique_ptr<PanelPair> bv_gen_key;
//	std::unique_ptr<PanelPair> bv_left;
//	std::unique_ptr<PanelPair> bv_playbackLeft;
//	std::unique_ptr<PanelPair> bv_props_bnr;
//	std::unique_ptr<PanelPair> bv_right;
//	std::unique_ptr<PanelPair> bv_RT_pair;
//	std::unique_ptr<PanelPair> bv_RTL_pair;
//	std::unique_ptr<PanelPair> bv_RTLT_pair;
//	std::unique_ptr<PanelPair> bv_RTLTB_pair;
//	std::unique_ptr<PanelPair> bv_RTLTBL_pair;
//	std::unique_ptr<PanelPair> bv_spectSurf;
//	std::unique_ptr<PanelPair> bv_topPair;
//	std::unique_ptr<PanelPair> bv_whole;

	std::unique_ptr<Bounded> envCtrlBounds;

	std::unique_ptr<TabbedSelector> topRightTabs;
	std::unique_ptr<TabbedSelector> bottomTabs;
	std::unique_ptr<TabbedSelector> topTabs;

	std::unique_ptr<BannerPanel> bannerPanel;

	std::unique_ptr<Bounded> keybBounds;
	std::unique_ptr<Bounded> genBounds;
	std::unique_ptr<Bounded> consBounds;
	std::unique_ptr<Bounded> modBounds;
	std::unique_ptr<Bounded> playbackBounds;

	std::unique_ptr<Bounded> spectCtrlBounds;
	std::unique_ptr<Bounded> surfCtrlBounds;
	std::unique_ptr<Bounded> guideCtrlBounds;
	std::unique_ptr<Bounded> tubeCtrlBoundsB;

//	std::unique_ptr<Bounded> tubeCtrlBounds;
//	std::unique_ptr<Bounded> envCtrlABounds;
//	std::unique_ptr<Bounded> envCtrlBBounds;

//	std::unique_ptr<ZoomPanel> spectZoomPanel;
//	std::unique_ptr<ZoomPanel> e3dZoomPanel;
//	std::unique_ptr<ZoomPanel> envZoomPanel;
//	std::unique_ptr<ZoomPanel> f2dZoomPanel;
//	std::unique_ptr<ZoomPanel> wave3DZoomPanel;
//	std::unique_ptr<ZoomPanel> wave2DZoomPanel;
//	std::unique_ptr<ZoomPanel> tubeZoomPanel;
//	std::unique_ptr<ZoomPanel> wsZoomPanel;
//	std::unique_ptr<ZoomPanel> dfrmZoomPanel;

	std::unique_ptr<Bounded> derivBounds;
	std::unique_ptr<Bounded> propsBounds;
	std::unique_ptr<Bounded> unisonBounds;
	std::unique_ptr<Bounded> bnrBounds;
	std::unique_ptr<Bounded> menuBounds;
	std::unique_ptr<Bounded> oscCtrlBounds;
//	std::unique_ptr<Bounded> bottomCtrlBounds;
	std::unique_ptr<Bounded> reverbBounds;
	std::unique_ptr<Bounded> delayBounds;
	std::unique_ptr<Bounded> eqBounds;
	std::unique_ptr<Bounded> wsCtrlBounds;
//	std::unique_ptr<Bounded> wave2DCtrlBounds;
	std::unique_ptr<Bounded> cv_botTabBounds;
	std::unique_ptr<Bounded> cv_topTabBounds;

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

	std::unique_ptr<MenuBarComponent> menuBar;

	std::unique_ptr<Dragger> xv_topBotDragger;
	std::unique_ptr<Dragger> xv_spectSurfDragger;
	std::unique_ptr<Dragger> xv_envDfmImpDragger;
	std::unique_ptr<Dragger> xv_dfmImpDragger;
	std::unique_ptr<Dragger> xv_wholeDragger;

	std::unique_ptr<Dragger> cv_wholeDragger;
	std::unique_ptr<Dragger> cv_middleDragger;

	std::unique_ptr<Dragger> cv_envSpectDragger;
	std::unique_ptr<Dragger> cv_spectSurfDragger;

//	std::unique_ptr<OpenGLPanel> crsGL;
//	std::unique_ptr<OpenGLPanel> f2GL;
//	std::unique_ptr<OpenGLPanel> e2GL;
//	std::unique_ptr<OpenGLPanel> dfmGL;
//	std::unique_ptr<OpenGLPanel> wsGL;
//	std::unique_ptr<OpenGLPanel> tmGL;
//
//	std::unique_ptr<OpenGLPanel3D> surfGL;
//	std::unique_ptr<OpenGLPanel3D> f3GL;
//	std::unique_ptr<OpenGLPanel3D> e3GL;

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

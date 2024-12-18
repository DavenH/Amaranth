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

	Bounded* presetPageBounds;
//	PanelPair* wave2DTabs;

	PanelPair* wavePair;
	PanelPair* spectPair;
	PanelPair* envPair;

	PanelPair* ev_bottomRight;
	PanelPair* ev_reverbUni;
	PanelPair* ev_delayEQ;
	PanelPair* ev_paramFX;
	PanelPair* ev_tubeParamFX;
	PanelPair* cv_bottomPair;
	PanelPair* cv_middlePair;

	PanelPair* wave2DPair;
	PanelPair* dfrmPair;
	PanelPair* irmodPair;
	PanelPair* wshpPair;

	// main view mode
	PanelPair* cv_bnrOscCtrls;
	PanelPair* cv_genKeyBnr;
	PanelPair* cv_leftTriple;
//	PanelPair* cv_keyBnr;
//	PanelPair* cv_f2dKeyGen;
//	PanelPair* cv_topRight;
//	PanelPair* cv_propsMod;
	PanelPair* cv_menuCons;
	PanelPair* cv_playbackLeft;
	PanelPair* cv_right;
	PanelPair* cv_rt_pair;
	PanelPair* cv_rtb_pair;
	PanelPair* cv_rtt_pair;
	PanelPair* cv_rttb_pair;
	PanelPair* cv_spectSurf;
	PanelPair* cv_whole;

	PanelPair* xv_ctrl_key;
	PanelPair* xv_envDfmImp;
	PanelPair* xv_dfrm_imp;
	PanelPair* xv_FX_pair_1;
	PanelPair* xv_FX_Pair_2;
	PanelPair* xv_FX_pair_A;
	PanelPair* xv_TRTRBR_pair;
	PanelPair* xv_TRTRB_pair;
	PanelPair* xv_TRTR_pair;
	PanelPair* xv_TRBR_pair;
	PanelPair* xv_TRBL_pair;
	PanelPair* xv_TRT_pair;
	PanelPair* xv_TRB_pair;
	PanelPair* xv_TR_pair;
	PanelPair* xv_topPair;
	PanelPair* xv_whole;
	PanelPair* xv_spectSurf;
	PanelPair* xv_playbackLeft;

//	PanelPair* bv_dfrm_imp;
//	PanelPair* bv_envDfmImp;
//	PanelPair* bv_FX_pair_1;
//	PanelPair* bv_FX_Pair_2;
//	PanelPair* bv_FX_pair_A;
//	PanelPair* bv_FX_pair;
//	PanelPair* bv_gen_key;
//	PanelPair* bv_left;
//	PanelPair* bv_playbackLeft;
//	PanelPair* bv_props_bnr;
//	PanelPair* bv_right;
//	PanelPair* bv_RT_pair;
//	PanelPair* bv_RTL_pair;
//	PanelPair* bv_RTLT_pair;
//	PanelPair* bv_RTLTB_pair;
//	PanelPair* bv_RTLTBL_pair;
//	PanelPair* bv_spectSurf;
//	PanelPair* bv_topPair;
//	PanelPair* bv_whole;

	Bounded* envCtrlBounds;

	TabbedSelector* topRightTabs;
	TabbedSelector* bottomTabs;
	TabbedSelector* topTabs;

	BannerPanel* bannerPanel;

	Bounded* keybBounds;
	Bounded* genBounds;
	Bounded* consBounds;
	Bounded* modBounds;
	Bounded* playbackBounds;

	Bounded* spectCtrlBounds;
	Bounded* surfCtrlBounds;
	Bounded* guideCtrlBounds;
	Bounded* tubeCtrlBoundsB;

//	Bounded* tubeCtrlBounds;
//	Bounded* envCtrlABounds;
//	Bounded* envCtrlBBounds;

	ZoomPanel* spectZoomPanel;
	ZoomPanel* e3dZoomPanel;
	ZoomPanel* envZoomPanel;
	ZoomPanel* f2dZoomPanel;
	ZoomPanel* wave3DZoomPanel;
	ZoomPanel* wave2DZoomPanel;
	ZoomPanel* tubeZoomPanel;
	ZoomPanel* wsZoomPanel;
	ZoomPanel* dfrmZoomPanel;

	Bounded* derivBounds;
	Bounded* propsBounds;
	Bounded* unisonBounds;
	Bounded* bnrBounds;
	Bounded* menuBounds;
	Bounded* oscCtrlBounds;
//	Bounded* bottomCtrlBounds;
	Bounded* reverbBounds;
	Bounded* delayBounds;
	Bounded* eqBounds;
	Bounded* wsCtrlBounds;
//	Bounded* wave2DCtrlBounds;
	Bounded* cv_botTabBounds;
	Bounded* cv_topTabBounds;

	MenuBarComponent* menuBar;

	Dragger* xv_topBotDragger;
	Dragger* xv_spectSurfDragger;
	Dragger* xv_envDfmImpDragger;
	Dragger* xv_dfmImpDragger;
	Dragger* xv_wholeDragger;

	Dragger* cv_wholeDragger;
	Dragger* cv_middleDragger;

	Dragger* cv_envSpectDragger;
	Dragger* cv_spectSurfDragger;

	OpenGLPanel* crsGL;
	OpenGLPanel* f2GL;
	OpenGLPanel* e2GL;
	OpenGLPanel* dfmGL;
	OpenGLPanel* wsGL;
	OpenGLPanel* tmGL;
	OpenGLPanel3D* surfGL;
	OpenGLPanel3D* f3GL;
	OpenGLPanel3D* e3GL;

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

	PanelGroup wave2DGroup, surfGroup, spectGroup2, spectGroup3, envGroup2, envGroup3, wshpGroup, irGroup, dfrmGroup;
	Array<PanelGroup*> panelGroups;

	OwnedArray<void*> deletable;

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

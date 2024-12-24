#include <Algo/PitchTracker.h>
#include <App/AppConstants.h>
#include <App/EditWatcher.h>
#include <Audio/AudioHub.h>
#include <Audio/Multisample.h>
#include <Design/Updating/Updater.h>
#include <Inter/Interactor.h>
#include <UI/Panels/Panel.h>
#include <UI/Panels/Panel3D.h>

#include "CycleTour.h"
#include "Dialogs.h"
#include "Directories.h"
#include "FileManager.h"
#include "Initializer.h"

#include <Util/StatusChecker.h>

#include "KeyboardInputHandler.h"
#include "WaveDragTarget.h"

#include "../Audio/AudioSourceRepo.h"
#include "../Audio/SampleUtils.h"
#include "../Audio/SynthAudioSource.h"
#include "../Curve/E3Rasterizer.h"
#include "../Curve/GraphicRasterizer.h"
#include "../CycleDefs.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../Inter/EnvelopeInter3D.h"
#include "../Inter/SpectrumInter2D.h"
#include "../Inter/SpectrumInter3D.h"
#include "../Inter/WaveformInter2D.h"
#include "../Inter/WaveformInter3D.h"
#include "../UI/CycleGraphicsUtils.h"
#include "../UI/Dialogs/PresetPage.h"
#include "../UI/Dialogs/QualityDialog.h"
#include "../UI/Effects/DelayUI.h"
#include "../UI/Effects/EffectGuiRegistry.h"
#include "../UI/Effects/EqualizerUI.h"
#include "../UI/Effects/IrModellerUI.h"
#include "../UI/Effects/ReverbUI.h"
#include "../UI/Effects/UnisonUI.h"
#include "../UI/Effects/WaveshaperUI.h"
#include "../UI/Layout/ResizerPullout.h"
#include "../UI/Panels/Console.h"
#include "../UI/Panels/DerivativePanel.h"
#include "../UI/Panels/GeneralControls.h"
#include "../UI/Panels/MainPanel.h"
#include "../UI/Panels/ModMatrixPanel.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/Panels/OscControlPanel.h"
#include "../UI/Panels/PlaybackPanel.h"
#include "../UI/Panels/PlayerComponent.h"
#include "../UI/Panels/SynthMenuBarModel.h"
#include "../UI/Panels/VertexPropertiesPanel.h"
#include "../UI/VertexPanels/DeformerPanel.h"
#include "../UI/VertexPanels/Envelope2D.h"
#include "../UI/VertexPanels/Envelope3D.h"
#include "../UI/VertexPanels/Spectrum2D.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VertexPanels/Waveform2D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"
#include "../UI/Widgets/MidiKeyboard.h"
#include "../Updating/CycleUpdater.h"
#include "../Util/CycleEnums.h"
#include "VersionConsiliator.h"

Atomic<int> Initializer::numInstances = Atomic<int>(int(0));


Initializer::Initializer() :
        SingletonAccessor(nullptr, "Initializer")
    ,	instanceId  		(0)
    , 	haveInitDsp			(false)
    , 	haveInitGfx			(false)
    , 	haveFreedResources	(false) {
    singletonRepo = std::make_unique<SingletonRepo>();
    repo = singletonRepo.get();
}

void Initializer::init() {
    if (haveInitDsp) {
        return;
    }

    haveInitDsp = true;

    if (numInstances.get() == 0) {
        ++numInstances;

        ippInit();
        Curve::calcTable();
    } else {
        ++numInstances;
    }

    instanceId = numInstances.get();

    status(ippSetNumThreads(1));
    status(ippSetDenormAreZeros(1));

    repo->instantiate();
    setConstants();
    setDefaultSettings();
    instantiate();

    repo->init();
    init2();

    if (instanceId == 1) {
        getObj(PresetPage).downloadCommunityDocumentDetails();
    }

    getObj(Dialogs).showDeviceErrorApplicably();
}

void Initializer::init2() {
    KeyboardInputHandler* handler 	= &getObj(KeyboardInputHandler);
    MainPanel* main 				= &getObj(MainPanel);
    MorphPanel* morph 				= &getObj(MorphPanel);
    MeshLibrary* meshLib			= &getObj(MeshLibrary);

    main->addKeyListener(handler);
    morph->addKeyListener(handler);

    getObj(PresetPage).addKeyListener(handler);
    getObj(GeneralControls).addKeyListener(handler);
    getObj(VertexPropertiesPanel).addKeyListener(handler);

    repo->setMorphPositioner(morph);
    repo->setConsole(&getObj(IConsole));
    repo->setDeformer(&getObj(DeformerPanel));

    int width, height;

    Dialogs::getSizeFromSetting(getSetting(WindowSize), width, height);

    meshLib->addListener(&getObj(CycleUpdater));
    meshLib->addListener(&getObj(ModMatrixPanel));
    meshLib->addListener(&getObj(VersionConsiliator));

    meshLib->addGroup(LayerGroups::GroupVolume);
    meshLib->addGroup(LayerGroups::GroupPitch);
    meshLib->addGroup(LayerGroups::GroupScratch);
    meshLib->addGroup(LayerGroups::GroupDeformer);
    meshLib->addGroup(LayerGroups::GroupTime);
    meshLib->addGroup(LayerGroups::GroupSpect);
    meshLib->addGroup(LayerGroups::GroupPhase);
    meshLib->addGroup(LayerGroups::GroupWavePitch);
    meshLib->addGroup(LayerGroups::GroupWaveshaper);
    meshLib->addGroup(LayerGroups::GroupIrModeller);

    meshLib->addLayer(LayerGroups::GroupVolume);
    meshLib->addLayer(LayerGroups::GroupPitch);
    meshLib->addLayer(LayerGroups::GroupScratch);
    meshLib->addLayer(LayerGroups::GroupDeformer);
    meshLib->addLayer(LayerGroups::GroupTime);
    meshLib->addLayer(LayerGroups::GroupSpect);
    meshLib->addLayer(LayerGroups::GroupPhase);
    meshLib->addLayer(LayerGroups::GroupWavePitch);
    meshLib->addLayer(LayerGroups::GroupWaveshaper);
    meshLib->addLayer(LayerGroups::GroupIrModeller);

    morph->updateCube();
    main->switchedRenderingMode(false);
    main->setSize(width, height);

    /*
  #if ! PLUGIN_MODE
    audioMgr	->init();
    settings	->initialiseAudioSettings();
  #endif

    mainPanel	->init();
    mainPanel	->switchedRenderingMode(false);

    editWatcher	->init();
    mainPanel	->setSize(width, height);

    mainPanel	->addKeyListener(inputHandler);
    presetPage	->addKeyListener(inputHandler);
    propsPanel	->addKeyListener(inputHandler);
    modPanel	->addKeyListener(inputHandler);
    genControls	->addKeyListener(inputHandler);

    resizeVertices();

    modPanel->updateCube();
    operations->openDefaultPreset();
    operations->getUndoManager().clearUndoHistory();
//	operations->openWave(File("G:\\samples\\modelling\\95238__corsica-s__oh-yeah.wav"), GlobalOperations::DialogSource);
 */

}

void Initializer::initSingletons() {
}

void Initializer::setConstants() {
    using namespace Constants;

    const String fontFace = platformSplit("Verdana", "Helvetica", "Noto Sans");

    auto &constants = getObj(AppConstants);
    constants.setConstant(WaveshaperPadding, 	0.0625);
    constants.setConstant(IrModellerPadding, 	0.0625);
    constants.setConstant(DocMagicCode, 	 	(int) 0xc0dedbad);
    constants.setConstant(ProductName, 	 		String(ProjectInfo::projectName));
    constants.setConstant(DocumentExt, 	 		"cyc");
    constants.setConstant(NumVoices, 	 		12);
    constants.setConstant(MaxUnisonOrder, 		10);
    constants.setConstant(MaxCyclePeriod, 		4096);
    constants.setConstant(FFTLogTensionAmp, 	500);
    constants.setConstant(EnvResolution, 		128);
    constants.setConstant(ResamplerLatency, 	32);
    constants.setConstant(MinLineLength, 		0.001);
    constants.setConstant(FontFace,             fontFace);
}

void Initializer::setDefaultSettings() {
    const String path = platformSplit(
        "/Amaranth Audio/Cycle/install.xml",
        "/Preferences/com.amaranthaudio.Cycle.xml",
        ""
    );
    getObj(Settings).setPropertiesPath(path);

    getSetting(IgnoringMessages) 		= true;
    getSetting(MagnitudeDrawMode) 		= true;
    getSetting(InterpWaveCycles) 		= true;
    getSetting(WrapWaveCycles) 			= true;
    getSetting(CollisionDetection) 		= true;
    getSetting(ViewVertsOnlyOnHover) 	= false;
    getSetting(DrawWave) 				= false;
    getSetting(Waterfall) 				= false;
    getSetting(WaveLoaded) 				= false;
    getSetting(DebugLogging) 			= false;
    getSetting(NativeDialogs) 			= false;
    getSetting(UseYellowDepth) 			= false;
    getSetting(UseRedDepth) 			= false;
    getSetting(UseBlueDepth) 			= false;
    getSetting(UseLargerPoints) 		= false;
    getSetting(ReductionFactor) 		= 5;
    getSetting(ViewStage) 				= ViewStages::PostFX;
    getSetting(WindowSize) 				= WindowSizes::FullSize;
    getSetting(PitchAlgo) 				= PitchAlgos::AlgoAuto;
    getSetting(CurrentEnvGroup) 		= LayerGroups::GroupVolume;
}

void Initializer::instantiate() {
    Interactor* timeInter, *spectInter;
    DeformerPanel* deformer;
    SynthAudioSource* audioSource;
    auto* pitchRast = new EnvWavePitchRast(repo, "EnvWavePitchRast");

    // APP
    repo->add(new Directories(repo), -50);
    repo->add(new FileManager(repo));
    repo->add(new KeyboardInputHandler(repo));
    repo->add(new VersionConsiliator(repo));

    // AUDIO
    repo->add(new SampleUtils(repo));
    repo->add(new AudioSourceRepo(repo));
    repo->add(new MidiKeyboard(repo, getObj(AudioHub).getKeyboardState(), MidiKeyboardComponent::horizontalKeyboard));
    repo->add(pitchRast);
    repo->add(audioSource = new SynthAudioSource(repo));
    repo->add(new Multisample(repo, pitchRast));

    auto* delay		 = new Delay(repo);
    auto* unison 	 = new Unison(repo);
    auto* reverb 	 = new ReverbEffect(repo);
    auto* equalizer  = new Equalizer(repo);
    auto* modeller 	 = new IrModeller(repo);
    auto* waveshaper = new Waveshaper(repo);

    repo->add(delay);
    repo->add(unison);
    repo->add(reverb);
    repo->add(equalizer);
    repo->add(modeller);
    repo->add(waveshaper);

    // owns lifetime
    audioSource->setUnison(unison);
    audioSource->setReverb(reverb);
    audioSource->setEqualizer(equalizer);
    audioSource->setWaveshaper(waveshaper);
    audioSource->setIrModeller(modeller);
    audioSource->setDelay(delay);

    // UI
    repo->add(new Dialogs			(repo));
    repo->add(new CycleTour			(repo));
    repo->add(new WaveDragTarget	(repo));
    repo->add(new Waveform2D		(repo));
    repo->add(new Spectrum2D		(repo));
    repo->add(new DerivativePanel	(repo));
    repo->add(new VertexPropertiesPanel(repo));
    repo->add(new GeneralControls	(repo));
    repo->add(new ResizerPullout	(repo));
    repo->add(new PlayerComponent	(repo));
    repo->add(new OscControlPanel	(repo));
    repo->add(new SynthMenuBarModel	(repo));
    repo->add(new Console			(repo));
    repo->add(new PresetPage		(repo));
    repo->add(new QualityDialog		(repo));
    repo->add(new EffectGuiRegistry	(repo));
    repo->add(new CycleGraphicsUtils(repo));
    repo->add(new Waveform3D		(repo), 10);
    repo->add(new Spectrum3D		(repo), 10);
    repo->add(new MainPanel			(repo), 100);
    repo->add(new MorphPanel		(repo));
    repo->add(new PlaybackPanel		(repo));
    repo->add(new ModMatrixPanel	(repo));

    auto* delayUI 		= new DelayUI(repo, delay);
    auto* unisonUI 		= new UnisonUI(repo, unison);
    auto* reverbUI 		= new ReverbUI(repo, reverb);
    auto* equalizerUI	= new EqualizerUI(repo, equalizer);
    auto* modellerUI	= new IrModellerUI(repo);
    auto* wshpUI		= new WaveshaperUI(repo);

    delay->setUI(delayUI);
    unison->setUI(unisonUI);
    reverb->setUI(reverbUI);
    equalizer->setUI(equalizerUI);
    modeller->setUI(modellerUI);
    waveshaper->setUI(wshpUI);

    repo->add(delayUI);
    repo->add(reverbUI);
    repo->add(unisonUI);
    repo->add(equalizerUI);
    repo->add(modellerUI);
    repo->add(wshpUI);

    // GRAPHIC DSP
    repo->add(new VisualDsp(repo));
    repo->add(new E3Rasterizer(repo));

    // WORKFLOW
    repo->add(new EnvelopeInter3D(repo));
    repo->add(new WaveformInter3D(repo));
    repo->add(new SpectrumInter3D(repo));

    repo->add(new EnvelopeInter2D(repo));
    repo->add(new CycleUpdater(repo));
    repo->add(timeInter  = new WaveformInter2D(repo));
    repo->add(spectInter = new SpectrumInter2D(repo));
    repo->add(deformer 	 = new DeformerPanel(repo));

    repo->add(new Envelope2D(repo));
    repo->add(new Envelope3D(repo));

    repo->add(new TimeRasterizer (repo, timeInter, 	"TimeRasterizer", 	LayerGroups::GroupTime,  true, 0));
    repo->add(new SpectRasterizer(repo, spectInter, "SpectRasterizer", 	LayerGroups::GroupSpect, true, 0));
    repo->add(new PhaseRasterizer(repo, spectInter, "PhaseRasterizer", 	LayerGroups::GroupPhase, true, 0));

    repo->add(new EnvVolumeRast  (repo, deformer, "EnvVolumeRast"));
    repo->add(new EnvPitchRast   (repo, deformer, "EnvPitchRast"));
    repo->add(new EnvScratchRast (repo, deformer, "EnvScratchRast"));

    repo->add(this);
}

Initializer::~Initializer() {
    progressMark

    if(Component* c = getObj(PresetPage).getParentComponent()) {
        c->exitModalState(1);
    }

  #if ! PLUGIN_MODE
    getObj(SynthAudioSource).allNotesOff();
    getObj(AudioHub).stopAudio();
  #endif

    --numInstances;

    if (numInstances.get() == 0) {
        Curve::deleteTable();
    }

    // must delete singletons and everything that will use ipp_cyc.dll before we unload it
    singletonRepo = nullptr;
    repo = nullptr;

  #if PLUGIN_MODE
    #ifdef JUCE_WINDOWS
    if(numInstances.get() == 0)
    {
        if(HMODULE module = GetModuleHandleA(dllName.toUTF8()))
            FreeLibrary(module);
    }
    #endif
  #endif
}

void Initializer::resetAll() {
    progressMark

    repo->resetSingletons();
    doUpdate(SourceAll);
}

void Initializer::takeLocks() {
    for (int i = 0; i < lockingPanels.size(); ++i) {
        lockingPanels.getUnchecked(i)->getRenderLock().enter();
    }
}

void Initializer::releaseLocks() {
    for (int i = lockingPanels.size() - 1; i >= 0; --i) {
        lockingPanels.getUnchecked(i)->getRenderLock().enter();
    }
}

void Initializer::freeUIResources() {
    info("freeing UI resources\n");

    getObj(Waveform3D).deactivateContext();
    getObj(Waveform2D).deactivateContext();
    getObj(Spectrum3D).deactivateContext();
    getObj(Spectrum2D).deactivateContext();
    getObj(Envelope2D).deactivateContext();
    getObj(Envelope3D).deactivateContext();
    getObj(WaveshaperUI).deactivateContext();
    getObj(IrModellerUI).deactivateContext();
    getObj(DeformerPanel).deactivateContext();
    getObj(VisualDsp).reset();
    getObj(Waveform3D).freeResources();
    getObj(Spectrum3D).freeResources();
    getObj(Envelope3D).freeResources();

    haveFreedResources = true;
}

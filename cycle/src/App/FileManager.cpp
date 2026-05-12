#include <Algo/PitchTracker.h>
#include <App/AppConstants.h>
#include <App/Doc/Document.h>
#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/AudioHub.h>
#include <Audio/Multisample.h>
#include <Design/Updating/Updater.h>
#include <UI/Panels/Panel2D.h>
#include <Util/ScopedFunction.h>

#include "CycleTour.h"
#include "Directories.h"
#include "Dialogs.h"
#include "MeshDefaults.h"
#include "FileManager.h"

#include "../App/Initializer.h"
#include "../Audio/AudioSourceRepo.h"
#include <Audio/PluginProcessor.h>
#include "../Audio/WavAudioSource.h"
#include "../Audio/SampleUtils.h"
#include "../Audio/SynthAudioSource.h"
#include "../CycleDefs.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../UI/Dialogs/PresetPage.h"
#include "../UI/Effects/IrModellerUI.h"
#include "../UI/Effects/UnisonUI.h"
#include "../UI/Effects/WaveshaperUI.h"
#include "../UI/Panels/GeneralControls.h"
#include "../UI/Panels/MainPanel.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/Panels/PlaybackPanel.h"
#include "../UI/Panels/VertexPropertiesPanel.h"
#include "../UI/VertexPanels/GuideCurvePanel.h"
#include "../UI/VertexPanels/Envelope2D.h"
#include "../UI/VertexPanels/Envelope3D.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"
#include "../Util/CycleEnums.h"
#include <Definitions.h>

FileManager::FileManager(SingletonRepo* repo) :
        SingletonAccessor(repo, "FileManager")
    ,	shouldOpenDefaultPreset(true) {
  #if PLUGIN_MODE
    defaultPresetName = "empty";
  #else
    defaultPresetName = "ooh-aah";
  #endif

  #ifdef _DEBUG
    // defaultPresetName = "CalmingKeys";
    // defaultPresetName = "Empty";
  #endif

    DBG("FileManager ctor defaultPresetName=\"" + defaultPresetName + "\"");
}

void FileManager::loadPendingItem() {
}

File FileManager::findFactoryPresetFile(const String& presetName) const {
    String filename = presetName + "." + getStrConstant(DocumentExt);
    StringArray searchDirs = getObj(Directories).getPresetSearchDirs();
    // DBG("FileManager::findFactoryPresetFile preset=\"" + presetName + "\" filename=\"" + filename + "\"");

    for (auto& dir : searchDirs) {
        File file(dir + filename);
        // DBG("  checking " + file.getFullPathName() + " exists=" + String(file.existsAsFile() ? "true" : "false"));

        if (file.existsAsFile()) {
            // DBG("  found preset at " + file.getFullPathName());
            return file;
        }
    }

    DBG("  no preset found for \"" + presetName + "\"");
    return {};
}

void FileManager::openFactoryPreset(const String &presetName) {
    DBG("FileManager::openFactoryPreset presetName=\"" + presetName + "\"");
    File file(findFactoryPresetFile(presetName));

    if (!file.existsAsFile()) {
        DBG("FileManager::openFactoryPreset failed to resolve presetName=\"" + presetName + "\"");
        jassertfalse;
        showConsoleMsg("Couldn't find factory preset: " + presetName);
        return;
    }

    openPreset(file);
}

void FileManager::openPreset(const File &file) {
    currentPresetName = file.getFullPathName();
    DBG("FileManager::openPreset currentPresetName=\"" + currentPresetName + "\"");

    info("Opening preset: " << currentPresetName << "\n");
    openCurrentPreset();
}

void FileManager::saveCurrentPreset() {
    if (canSaveOverCurrentPreset()) {
        getObj(Document).save(currentPresetName);
        getObj(EditWatcher).reset();
    } else {
        getObj(Dialogs).showPresetSaveAsDialog();
    }
}

void FileManager::openCurrentPreset() {
    progressMark
    DBG("FileManager::openCurrentPreset currentPresetName=\"" + currentPresetName + "\"");

    getSetting(IgnoringMessages) = true;
    getSetting(IgnoringEditMessages) = true;
    getObj(Updater).clearPendingUpdates();

    if(getSetting(DrawWave)) {
        getObj(SampleUtils).waveOverlayChanged(false);
    }

    getObj(SynthAudioSource).allNotesOff();

    formatSplit(getObj(AudioHub).suspendAudio(),
                getObj(PluginProcessor).suspendProcessing(true));

    unloadWav(false);

    auto& initializer = getObj(Initializer);
    ScopedLambda uiLocks([&initializer] { initializer.takeLocks(); },
                         [&initializer] { initializer.releaseLocks(); });

    // anything that depends on mesh or xml needs to be locked

    DBG("FileManager::openCurrentPreset calling Document::open");
    getObj(Document).open(currentPresetName);
    DBG("FileManager::openCurrentPreset Document::open returned");
    doPostPresetLoad();
    DBG("FileManager::openCurrentPreset finish post-preset updates");
}

bool FileManager::canSaveOverCurrentPreset() {
    DocumentDetails deets = getObj(Document).getDetails();
    String presetAlias = getObj(Settings).getProperty("AuthorAlias", "Anonymous");

    bool showSaveDialog = false;

    // a backdoor condition to help with saving factory presets
    showSaveDialog	|= currentPresetName.endsWithIgnoreCase("empty." + getStrConstant(DocumentExt));

    return ! showSaveDialog;
}

void FileManager::doPostPresetLoad() {
    progressMark

    auto& source = getObj(SynthAudioSource);
    auto& meshLibrary = getObj(MeshLibrary);

    for (int group : { LayerGroups::GroupGuideCurve, LayerGroups::GroupWaveshaper, LayerGroups::GroupIrModeller }) {
        auto& layerGroup = meshLibrary.getLayerGroup(group);

        for (auto& layer : layerGroup.layers) {
            MeshDefaults::migrateLegacyPaddingIfNeeded(repo, group, layer.mesh);
        }
    }

    source.setEnvelopeMeshes(true);
    source.enablementChanged();
    source.controlFreqChanged();
    source.setPendingGlobalChange();
    source.setPendingModRoute();
    source.updateTempoScale();
    source.prepNewVoice();

    getObj(VertexPropertiesPanel).updateComboBoxes();
    getObj(Spectrum3D)		.validateScratchChannels();
    getObj(Waveform3D)		.validateScratchChannels();
    getObj(GuideCurvePanel)	.rasterizeAllTables();

    if (Mesh* irMesh = meshLibrary.getCurrentMesh(LayerGroups::GroupIrModeller)) {
        getObj(IrModellerUI).setMeshAndUpdate(irMesh, false);
    }

    if (Mesh* waveshaperMesh = meshLibrary.getCurrentMesh(LayerGroups::GroupWaveshaper)) {
        getObj(WaveshaperUI).setMeshAndUpdate(waveshaperMesh);
    }

    getObj(IrModellerUI)	.updateDspSync();
    getObj(WaveshaperUI)	.updateDspSync();
    getObj(Waveform3D)		.updateBackground(false);
    getObj(Spectrum3D)		.updateBackground(false);
    getObj(Envelope3D)		.updateBackground(false);
    getObj(Waveform3D)		.getZoomPanel()->zoomToFull();
    getObj(Envelope2D)		.contractToRange(true);
    getObj(PresetPage)		.updatePresetIndex();
    getObj(MorphPanel)		.setSelectedCube(nullptr, nullptr, -1, false);

    if (getSetting(WaveLoaded)) {
        auto& multisample = getObj(Multisample);

        for (int i = 0; i < multisample.size(); ++i) {
            if (PitchedSample* sample = multisample.getSampleAt(i)) {
                auto& pitchRast = getObj(EnvPitchRast);
                sample->createPeriodsFromEnv(meshLibrary, &pitchRast);
            }
        }

        multisample.performUpdate(Update);
    }

  #if PLUGIN_MODE
    getObj(PluginProcessor).suspendProcessing(false);
    getObj(PluginProcessor).documentHasLoaded();
    getObj(PluginProcessor).updateLatency();
  #else
    getObj(AudioSourceRepo)	.setAudioProcessor(AudioSourceRepo::SynthSource);
    getObj(AudioHub)		.resumeAudio();
  #endif

    getObj(EnvelopeInter2D)	.switchedEnvelope(LayerGroups::GroupVolume, false, true);
    getObj(EnvelopeInter2D)	.enablementsChanged();
    getObj(EnvelopeInter2D)	.waveOverlayChanged();
    getObj(MeshLibrary)		.layerChanged(LayerGroups::GroupScratch, -1);
    getObj(MeshLibrary)		.layerChanged(LayerGroups::GroupGuideCurve, -1);
    getObj(GeneralControls)	.updateHighlights();
    getObj(GeneralControls)	.repaint();

    getSetting(IgnoringMessages) = false;
    getSetting(IgnoringEditMessages) = false;

    getObj(Initializer).resetAll(true);
    getObj(Waveform3D).reconcileLoadedState();
    getObj(Spectrum3D).reconcileLoadedState();
    getObj(UnisonUI).reconcileLoadedState(true);
    getObj(EditWatcher).update();
}

bool FileManager::openWave(const File &file, Dialogs::OpenWaveInvoker invoker, int defaultNote) {
    progressMark

    bool isMulti = false;

    WavAudioSource* wavSource = getObj(AudioSourceRepo).getWavAudioSource();
    wavSource->allNotesOff();

    ScopedLock sl(getObj(AudioSourceRepo).getWavAudioSource()->getLock());

    auto& multi = getObj(Multisample);

    if (file.existsAsFile()) {
        multi.addSample(file, defaultNote);
    } else {
        multi.createFromDirectory(file);
        isMulti = true;
    }

    if (invoker == Dialogs::DialogSource) {
        if (multi.getGreatestLengthSeconds() < 0.05f) {
            showImportant("Wave file too short.");
            return false;
        }
    }

    getSetting(DrawWave) 	= invoker == Dialogs::DialogSource;
    getSetting(WaveLoaded) 	= true;

    getObj(GeneralControls).updateHighlights();
    getObj(GeneralControls).repaint();

    if (invoker == Dialogs::DialogSource) {
        getObj(SampleUtils).processWav(isMulti, true);
        getObj(MainPanel).setPrimaryDimension(Vertex::Time, false);
        getObj(SampleUtils).waveOverlayChanged(true);
        getObj(PlaybackPanel).resetPlayback(false);

        doUpdate(SourceMorph);
    } else if (invoker == Dialogs::PresetSource) {
        ScopedValueSetter<int> waveFlag(getSetting(DrawWave), true, false);

        getObj(EnvelopeInter2D).waveOverlayChanged();
        getObj(SampleUtils).processWav(isMulti, false);
    }

    return true;
}

void FileManager::unloadWav(bool doUpdate) {
    progressMark

    getSetting(WaveLoaded) = false;
    getObj(Directories).setLoadedWave(String());

    if (doUpdate) {
        if (getSetting(DrawWave)) {
            getObj(VisualDsp).destroyArrays();
        }

        getObj(SampleUtils).waveOverlayChanged(false);
    } else {
        getSetting(DrawWave) = false;
    }
}

void FileManager::openDefaultPreset() {
    DBG("FileManager::openDefaultPreset shouldOpenDefaultPreset=" + String(shouldOpenDefaultPreset ? "true" : "false"));
    // some hosts want to call this after loading the state block,
    // which overwrites the preset
    if (shouldOpenDefaultPreset) {
        getSetting(IgnoringMessages) = false;

        String cmdUnquoted = getObj(Initializer).getCommandLine().trim().unquoted();
        DBG("FileManager::openDefaultPreset commandLine=\"" + cmdUnquoted + "\" defaultPresetName=\"" + defaultPresetName + "\"");

        bool commandLineIsPreset =
                cmdUnquoted.isNotEmpty() &&
                        cmdUnquoted.endsWithIgnoreCase(getStrConstant(DocumentExt));

        if (cmdUnquoted.isNotEmpty()) {
            info("Command line: " << cmdUnquoted << "\n");
        }

        if (commandLineIsPreset) {
            File openedPreset(cmdUnquoted);
            if (openedPreset.existsAsFile()) {
                DBG("FileManager::openDefaultPreset opening command-line preset " + openedPreset.getFullPathName());
                openPreset(openedPreset);
                getObj(Initializer).setCommandLine(String());

                return;
            }
            DBG("FileManager::openDefaultPreset command-line preset missing");
            showImportant(cmdUnquoted + " could not be opened");
        } else {
            DBG("FileManager::openDefaultPreset opening factory preset \"" + defaultPresetName + "\"");
            openFactoryPreset(defaultPresetName);
        }

        getObj(PresetPage).updateCurrentPreset(defaultPresetName);
    }

    // if (getSetting(FirstLaunch)) {
    //     noPlug(getObj(CycleTour).enter());
    // }
}

void FileManager::doPostWaveLoad(Dialogs::OpenWaveInvoker invoker) {
}

void FileManager::revertCurrentPreset() {
    if (getObj(EditWatcher).getHaveEdited()) {
        if (getObj(FileManager).getCurrentPresetName().isNotEmpty()) {
            auto* window = new AlertWindow("Revert Current Preset",
                                           "Please confirm you'd like to revert",
                                           AlertWindow::QuestionIcon, &getObj(MainPanel));
            window->setAlwaysOnTop(true);
            window->addButton("Cancel", Dialogs::DialogCancel, KeyPress(KeyPress::escapeKey));
            window->addButton("Revert", Dialogs::DialogSave);

            window->enterModalState(true, ModalCallbackFunction::create(revertCallback, repo.get()), true);
        }
    }
}

void FileManager::revertCallback(int returnId, SingletonRepo* repo) {
    if (returnId != Dialogs::DialogCancel) {
        getObj(EditWatcher).reset();
        getObj(FileManager).openCurrentPreset();
    }
}

#include <Definitions.h>
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
#include "FileManager.h"

#include "../App/Initializer.h"
#include "../Audio/AudioSourceRepo.h"
#include "../Audio/WavAudioSource.h"
#include "../Audio/SampleUtils.h"
#include "../Audio/SynthAudioSource.h"
#include "../CycleDefs.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../UI/Dialogs/PresetPage.h"
#include "../UI/Effects/IrModellerUI.h"
#include "../UI/Effects/WaveshaperUI.h"
#include "../UI/Panels/GeneralControls.h"
#include "../UI/Panels/MainPanel.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/Panels/PlaybackPanel.h"
#include "../UI/Panels/VertexPropertiesPanel.h"
#include "../UI/VertexPanels/DeformerPanel.h"
#include "../UI/VertexPanels/Envelope2D.h"
#include "../UI/VertexPanels/Envelope3D.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"
#include "../Util/CycleEnums.h"

FileManager::FileManager(SingletonRepo* repo) :
		SingletonAccessor(repo, "FileManager")
	,	shouldOpenDefaultPreset(true) {
  #if PLUGIN_MODE
	defaultPresetName = "Empty";
  #else
	defaultPresetName = "BaroqueFlute";
  #endif

  #ifdef _DEBUG
	defaultPresetName = "Empty";
  #endif
}

void FileManager::loadPendingItem() {
}

void FileManager::openFactoryPreset(const String &presetName) {
	File file(getObj(Directories).getPresetDir() + presetName + "." + getStrConstant(DocumentExt));

	openPreset(file);
}

void FileManager::openPreset(const File &file) {
    currentPresetName = file.getFullPathName();

    std::cout << "Opening preset: " << currentPresetName << "\n";
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

	if(getSetting(DrawWave))
		getObj(SampleUtils).waveOverlayChanged(false);

	getSetting(IgnoringMessages) = true;
	getObj(SynthAudioSource).allNotesOff();

	formatSplit(getObj(AudioHub).suspendAudio(),
				getObj(PluginProcessor).suspendProcessing(true));

	unloadWav(false);

	GlobalScopedFunction uiLocks(Initializer::takeLocks, Initializer::releaseLocks, repo);

	// anything that depends on mesh or xml needs to be locked

	getObj(Document).open(currentPresetName);
	doPostPresetLoad();
}


bool FileManager::canSaveOverCurrentPreset() {
	DocumentDetails deets 	= getObj(Document).getDetails();
	String presetAlias 		= getObj(Settings).getProperty("AuthorAlias", "Anonymous");

	bool showSaveDialog = false;

	// a backdoor condition to help with saving factory presets
	showSaveDialog	|= currentPresetName.endsWithIgnoreCase("empty." + getStrConstant(DocumentExt));

    if (presetAlias != "Daven") {
        showSaveDialog |= deets.getPack().equalsIgnoreCase("Factory");
	}

	return ! showSaveDialog;
}


void FileManager::doPostPresetLoad() {
    progressMark

	SynthAudioSource& source = getObj(SynthAudioSource);

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
	getObj(DeformerPanel)	.rasterizeAllTables();
	getObj(IrModellerUI)	.updateDspSync();
	getObj(WaveshaperUI)	.updateDspSync();
	getObj(Waveform3D)		.updateBackground(false);
	getObj(Spectrum3D)		.updateBackground(false);
	getObj(Envelope3D)		.updateBackground(false);
	getObj(Waveform3D)		.getZoomPanel()->zoomToFull();
	getObj(Envelope2D)		.contractToRange(true);
	getObj(PresetPage)		.updatePresetIndex();
	getObj(MorphPanel)		.setSelectedCube(nullptr, nullptr, -1, false);

  #if PLUGIN_MODE
	getObj(PluginProcessor)	.suspendProcessing(false);
	getObj(PluginProcessor)	.documentHasLoaded();
	getObj(PluginProcessor)	.updateLatency();
  #else
	getObj(AudioHub)		.resumeAudio();
	getObj(AudioSourceRepo)	.setAudioProcessor(AudioSourceRepo::SynthSource);
  #endif

	getObj(EnvelopeInter2D)	.switchedEnvelope(LayerGroups::GroupVolume, false, true);
	getObj(EnvelopeInter2D)	.waveOverlayChanged();
	getObj(MeshLibrary)		.layerChanged(LayerGroups::GroupScratch, -1);
	getObj(MeshLibrary)		.layerChanged(LayerGroups::GroupDeformer, -1);
	getObj(GeneralControls)	.updateHighlights();
	getObj(GeneralControls)	.repaint();

	getSetting(IgnoringMessages) = false;

	getObj(Updater).update(UpdateType::RestoreDetail, UpdateSources::SourceAll);
	getObj(Initializer).resetAll();
	getObj(EditWatcher).update();
}


bool FileManager::openWave(const File &file, Dialogs::OpenWaveInvoker invoker, int defaultNote) {
	progressMark

	bool isMulti = false;

	WavAudioSource* wavSource = getObj(AudioSourceRepo).getWavAudioSource();
	wavSource->allNotesOff();

	ScopedLock sl(getObj(AudioSourceRepo).getWavAudioSource()->getLock());

	Multisample& multi = getObj(Multisample);

	if(file.existsAsFile())
		multi.addSample(file, defaultNote);
	else
	{
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
    getObj(Directories).setLoadedWave(String::empty);

    if (doUpdate) {
        if (getSetting(DrawWave))
			getObj(VisualDsp).destroyArrays();

		getObj(SampleUtils).waveOverlayChanged(false);
	}
	else
	{
		getSetting(DrawWave) = false;
	}
}


void FileManager::openDefaultPreset() {
    // some hosts want to call this after loading the state block,
    // which overwrites the preset
    if (shouldOpenDefaultPreset) {
        getSetting(IgnoringMessages) = false;

		String cmdUnquoted = getObj(Initializer).getCommandLine().trim().unquoted();

		bool commandLineIsPreset =
				cmdUnquoted.isNotEmpty() &&
                        cmdUnquoted.endsWithIgnoreCase(getStrConstant(DocumentExt));

        if (cmdUnquoted.isNotEmpty()) {
            dout << "Command line: " << cmdUnquoted << "\n";
        }

        if (commandLineIsPreset) {
            File openedPreset(cmdUnquoted);
            if (openedPreset.existsAsFile()) {
                openPreset(openedPreset);
                getObj(Initializer).setCommandLine(String::empty);

                return;
            } else {
                showImportant(cmdUnquoted + " could not be opened");
            }
        } else {
			openFactoryPreset(defaultPresetName);
		}

		getObj(PresetPage).updateCurrentPreset(defaultPresetName);
	}

    if (getSetting(FirstLaunch)) {
        noPlug(getObj(CycleTour).enter());
	}


//	if(getObj(Settings).getValidationState() == SerialChecker::betaVersion)
//	{
//		int64 smallTime = 0.001 * Time::currentTimeMillis();
//		if(smallTime > getConstant(BetaExpiry))
//		{
//			showCritical("Beta expired, please update to the latest");
//		}
//	}
}


void FileManager::doPostWaveLoad(Dialogs::OpenWaveInvoker invoker) {
}


void FileManager::revertCurrentPreset() {
    if (getObj(EditWatcher).getHaveEdited()) {
        if (getObj(FileManager).getCurrentPresetName().isNotEmpty()) {
            AlertWindow* window = new AlertWindow("Revert Current Preset",
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

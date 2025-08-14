#include <Algo/AutoModeller.h>
#include <Algo/PitchTracker.h>
#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/AudioHub.h>
#include <Audio/AudioSourceProcessor.h>
#include <Audio/Multisample.h>
#include <Audio/PluginProcessor.h>
#include <Audio/PitchedSample.h>
#include <UI/Panels/ZoomPanel.h>
#include <UI/Widgets/IconButton.h>
#include <Wireframe/Env/EnvelopeMesh.h>
#include <Wireframe/Rasterizer/FXRasterizer.h>
#include "EnvelopeInter2D.h"
#include "../App/CycleTour.h"
#include "../App/Initializer.h"
#include "../Audio/SynthAudioSource.h"
#include "../Inter/WaveformInter3D.h"
#include "../CycleDefs.h"
#include "../UI/Panels/MainPanel.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/Panels/OscControlPanel.h"
#include "../UI/Panels/VertexPropertiesPanel.h"
#include "../UI/VertexPanels/Envelope2D.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VertexPanels/Waveform2D.h"
#include "../UI/VertexPanels/Waveform3D.h"

EnvelopeInter2D::EnvelopeInter2D(SingletonRepo* repo) : 
		Interactor2D		(repo, "EnvelopeInter2D", Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Red, Vertex::Blue))
	,	SingletonAccessor	(repo, "EnvelopeInter2D")
	,	LayerSelectionClient(repo)

	,	layerSelector(repo, this)
	,	addRemover(			  this, repo, "Scratch Channel")
	,	loopIcon(		4, 3, this, repo, "Toggle selected vertex as loop start", String(), "Toggle vertex as loop start (select one, at least 2 behind sustain)")
	,	sustainIcon(	5, 3, this, repo, "Toggle selected vertex as sustain", 	 String(), "Toggle vertex as sustain (select exactly one)")
	,	enableButton(	5, 5, this, repo, "Enable envelope")

	,	contractIcon(   6, 0, this, repo, "Contract to range")
	,	expandIcon(   	6, 1, this, repo, "Expand to full")

	,	configIcon( 	9, 1, this, repo, "Envelope Options")
	,	volumeIcon( 	7, 0, this, repo, "Volume Envelope")
	,	pitchIcon(		7, 1, this, repo, "Pitch Envelope")
	,	scratchIcon(	7, 2, this, repo, "Scratch Envelopes")
	,	wavePitchIcon(  7, 3, this, repo, "Wave Pitch Envelope", String(), "Wave pitch envelope - applicable when sample is loaded")
{
	updateSource 	= UpdateSources::SourceEnvelope2D;
	layerType 		= LayerGroups::GroupVolume;
	ignoresTime 	= true;
	scratchesTime 	= false;

	wavePitchIcon.setApplicable(false);
	collisionDetector.setNonintersectingDimension(CollisionDetector::Key);

	configIcon	 .setMouseScrollApplicable(false);
	volumeIcon	 .setMouseScrollApplicable(true);
	pitchIcon	 .setMouseScrollApplicable(true);
	scratchIcon	 .setMouseScrollApplicable(true);
	wavePitchIcon.setMouseScrollApplicable(true);

	vertexProps.ampVsPhaseApplicable = false;
	vertexProps.sliderApplicable[Vertex::Time] = false;
	enableButton.setHighlit(false);

	vertexProps.ampVsPhaseApplicable = true;

	vertexProps.dimensionNames.set(Vertex::Phase, "Time");
	vertexProps.dimensionNames.set(Vertex::Time, String());
	vertexProps.isEnvelope = true;
}

void EnvelopeInter2D::init() {
    envPanel = &getObj(Envelope2D);

    meshToSlider[LayerGroups::GroupVolume] = OscControlPanel::Volume;
    meshToSlider[LayerGroups::GroupPitch] = OscControlPanel::Octave;
    meshToSlider[LayerGroups::GroupScratch] = OscControlPanel::Speed;
    meshToSlider[LayerGroups::GroupWavePitch] = OscControlPanel::Octave;

    volumeIcon.setHighlit(true);
    loopIcon.setApplicable(false);

    vertexLimits[Vertex::Phase].setEnd(IPP_SQRT2); //envPanel->zoom.wLimit;
}

void EnvelopeInter2D::doExtraMouseUp() {
    vector < Vertex * > &selected = getSelected();
    EnvRasterizer* rast = getEnvRasterizer();
    EnvelopeMesh* envMesh = getCurrentMesh();

    if (envMesh == nullptr || rast == nullptr) {
        /*
         * Little bit of a hack (not going through the updater)
         */
        if (getSetting(CurrentEnvGroup) == LayerGroups::GroupWavePitch && getSetting(WaveLoaded)) {
            if (PitchedSample* sample = getObj(Multisample).getCurrentSample()) {
                sample->createPeriodsFromEnv(&getObj(EnvPitchRast));
            }

            doUpdate(SourceSpectrum3D);
        }

        return;
    }

    bool isSingleVertex = selected.size() == 1;
    bool selectedVertIsLoop = false;
    bool selectedVertIsSustain = false;
    bool isLoopApplicable = false;

    // make sure the indices are current
    if (!getSetting(UpdateGfxRealtime)) {
        rast->calcIntercepts();
        rast->evaluateLoopSustainIndices();
    }

    if (isSingleVertex) {
        Vertex* vert = selected.front();
        const vector <Intercept> &controlPoints = rasterizer->getRastData().intercepts;

        if (!controlPoints.empty()) {
            int icptIndex = -1;
            int sustIdx, loopIdx;
            rast->getIndices(loopIdx, sustIdx);

            for (int i = 0; i < (int) controlPoints.size(); ++i) {
                if (vert->isOwnedBy(controlPoints[i].cube))
                    icptIndex = i;
            }

            if (icptIndex >= 0 && icptIndex <= sustIdx - EnvRasterizer::loopMinSizeIcpts)
                isLoopApplicable = true;

            if (sustIdx >= 0 && loopIdx >= 0 && icptIndex >= 0 && sustIdx - loopIdx < EnvRasterizer::loopMinSizeIcpts) {
                envMesh->loopCubes.erase(controlPoints[icptIndex].cube);
                rast->evaluateLoopSustainIndices();

                envPanel->repaint();
                loopIdx = -1;
            }

            selectedVertIsLoop = icptIndex == loopIdx && icptIndex >= 0;
            selectedVertIsSustain = icptIndex == sustIdx && icptIndex >= 0;
        }
    }

    loopIcon.setApplicable(isLoopApplicable);
    loopIcon.setHighlit(selectedVertIsLoop);
    sustainIcon.setHighlit(selectedVertIsSustain);
}

void EnvelopeInter2D::setCurrentMesh(EnvelopeMesh* mesh) {
    if (mesh == nullptr) {
        return;
    }

    bool enabled = enableButton.isHighlit();
    if (EnvRasterizer* envRast = getEnvRasterizer()) {
        resetState();

        MeshLibrary::Layer &layer = getObj(MeshLibrary).getCurrentLayer(layerType);
        layer.mesh = mesh;

        getObj(EditWatcher).setHaveEditedWithoutUndo(true);
        envRast->setMesh(mesh);

        clearSelectedAndCurrent();
        updateHighlights();

        // do not lock the synth audio source CS.
        // It's already locked by the mesh selector calling this function
        getObj(SynthAudioSource).setEnvelopeMeshes(true);

        if (isCurrentMeshActive()) {
            triggerRefreshUpdate();
        } else {
            updateDspSync();
            performUpdate(Update);
        }
    }
}

void EnvelopeInter2D::previewMesh(EnvelopeMesh* mesh) {
    ScopedLock sl(panel->getRenderLock());

    if (EnvRasterizer* envRast = getEnvRasterizer()) {
        envRast->setMesh(mesh);
        envRast->performUpdate(Update);

        performUpdate(Update);
    }
}

void EnvelopeInter2D::previewMeshEnded(EnvelopeMesh* originalMesh) {
    ScopedLock sl(panel->getRenderLock());

    if (EnvRasterizer* envRast = getEnvRasterizer()) {
        envRast->setMesh(originalMesh);
        envRast->performUpdate(Update);

        performUpdate(Update);
    }
}

bool EnvelopeInter2D::isPitchEnvelope() {
    int currentEnv = getSetting(CurrentEnvGroup);

    return currentEnv == LayerGroups::GroupPitch || currentEnv == LayerGroups::GroupWavePitch;
}

EnvelopeMesh* EnvelopeInter2D::getCurrentMesh() {
    return dynamic_cast<EnvelopeMesh*>(Interactor::getMesh());
}

void EnvelopeInter2D::showCoordinates() {
    int envEnum = getSetting(CurrentEnvGroup);
    MeshLibrary::EnvProps* props = getObj(MeshLibrary).getCurrentEnvProps(envEnum);

    int beats = 4;
    float length = getObj(OscControlPanel).getLengthInSeconds();
    float tempoScale = getObj(SynthAudioSource).getTempoScale();

  #if PLUGIN_MODE
    AudioPlayHead::CurrentPositionInfo info = getObj(PluginProcessor).getCurrentPosition();
    beats = info.timeSigNumerator;
  #endif

    if (props->tempoSync) {
        length = beats * props->getEffectiveScale();
    }

    float time = length * state.currentMouse.x;
    bool sampleable = rasterizer->isSampleableAt(state.currentMouse.x);
    float envY = sampleable ? rasterizer->sampleAt(state.currentMouse.x) : 0;

    String yString;

    switch (envEnum) {
        case LayerGroups::GroupVolume:
            yString = Util::getDecibelString(envY);
            break;

        case LayerGroups::GroupPitch: {
            if (envY == 0 || envY == 1) {
                yString = "undefined";
            } else {
                double envSemis = NumberUtils::unitPitchToSemis(envY);
                int currentKey = getObj(MorphPanel).getCurrentMidiKey();
                double fineTune = NumberUtils::noteToFrequency(currentKey, envSemis * 100);

                yString =
                        (envSemis > 0 ? "+" : "") + String(envSemis, 2) + " semitones (" + String(fineTune, 3) + "Hz)";
            }

            break;
        }

        case LayerGroups::GroupWavePitch: {
            if (envY == 0 || envY == 1) {
                yString = "undefined";
            } else {
                if (PitchedSample* sample = getObj(Multisample).getCurrentSample()) {
                    double envSemis = NumberUtils::unitPitchToSemis(envY);
                    float key = sample->fundNote;
                    float value = NumberUtils::noteToFrequency(key, envSemis * 100);
                    float period = getObj(AudioHub).getSampleRate() / value;

                    yString = String(value, 3) + "Hz, " + String(period, 1) + " smp";
                }
            }

            break;
        }

        case LayerGroups::GroupScratch: {
            yString = String(envY * length, 4) + "s";

            break;
        }

        default:
            break;
    }

    String xString = props->tempoSync ? String(time, 2) + " beats, " : String(time, 4) + "s, ";
    String message = xString + yString;

    if (state.currentMouse.x > 1) {
        message += " (release region)";
    }

    showConsoleMsg(message);
}

// movingStart, as opposed to moving the sustain vertex which also wants synchronization
bool EnvelopeInter2D::synchronizeEnvPoints(Vertex* vertex, bool vertexIsLoopVert) {
    bool didAnything = false;

    if (EnvRasterizer* rast = getEnvRasterizer()) {
        const vector <Intercept> &controlPoints = rast->getRastData().intercepts;

        int loopIdx, sustIdx;
        rast->getIndices(loopIdx, sustIdx);

        TrilinearCube* sustainCube = sustIdx >= 0 ? controlPoints[sustIdx].cube : nullptr;
        TrilinearCube* loopCube = loopIdx >= 0 ? controlPoints[loopIdx].cube : nullptr;

        int numLoopPoints = sustIdx - loopIdx;

        if (sustainCube != nullptr && loopCube != nullptr) {
            TrilinearCube* toMove = vertexIsLoopVert ? sustainCube : loopCube;
            TrilinearCube* toCopyFrom = vertexIsLoopVert ? loopCube : sustainCube;

            for (int i = 0; i < TrilinearCube::numVerts; ++i) {
                toMove->getVertex(i)->values[Vertex::Amp] = toCopyFrom->getVertex(i)->values[Vertex::Amp];

                // if an component curve path is set, the sharpness is treated as its gain
                // plus, paths aren't smooth so disconts don't matter
                if (toMove->getCompPath() < 0) {
                    toMove->getVertex(i)->setMaxSharpness();
                }

                if (toCopyFrom->getCompPath() < 0) {
                    toCopyFrom->getVertex(i)->setMaxSharpness();
                }
            }

            didAnything = true;
        } else if (sustainCube != nullptr && sustainCube->pathAt(Vertex::Time) < 0) {
            for (int i = 0; i < TrilinearCube::numVerts; ++i) {
                sustainCube->getVertex(i)->setMaxSharpness();
            }
        }
    }

    return didAnything;
}


void EnvelopeInter2D::buttonClicked(Button* button) {
    progressMark

    if (button == &volumeIcon)          switchedEnvelope(LayerGroups::GroupVolume);
    else if (button == &pitchIcon)      switchedEnvelope(LayerGroups::GroupPitch);
    else if (button == &wavePitchIcon)  switchedEnvelope(LayerGroups::GroupWavePitch);
    else if (button == &scratchIcon)    switchedEnvelope(LayerGroups::GroupScratch);

    else if (button == &contractIcon || button == &expandIcon) {
        if (button == &contractIcon) {
            envPanel->zoomAndRepaint();
        } else {
            ZoomRect &rect = envPanel->getZoomPanel()->getZoomRect();
            rect.w = rect.xMaximum;
            rect.x = 0;

            envPanel->getZoomPanel()->panelZoomChanged(false);
            envPanel->repaint();
        }
    } else if (button == &addRemover.add || button == &addRemover.remove) {
        jassert(layerType == LayerGroups::GroupScratch);

        bool forceUpdate = false;
        bool globalUpdate = false;

        {
            ScopedLock lock(getObj(SynthAudioSource).getLock());

            if (button == &addRemover.add) {
                getObj(MeshLibrary).addLayer(layerType);
            } else if (button == &addRemover.remove) {
                MeshLibrary::LayerGroup &group = getObj(MeshLibrary).getLayerGroup(LayerGroups::GroupScratch);
                bool isLast = group.size() == 1;

                // also adjusts current layer
                getObj(MeshLibrary).removeLayerKeepingOne(LayerGroups::GroupScratch, group.current);

//				int layerIdx = getSetting(CurrentScratchLayer);
//				envPanel->removeScratchProps(layerIdx);

                if (!isLast) {
                    getObj(ModMatrixPanel).layerRemoved(LayerGroups::GroupScratch, group.current);
                }

                forceUpdate = true;
                globalUpdate |= group.current == 0;
            }

            getObj(SynthAudioSource).setEnvelopeMeshes(false);

            globalUpdate |= getObj(Spectrum3D).validateScratchChannels();
            globalUpdate |= getObj(Waveform3D).validateScratchChannels();
        }

        layerSelector.refresh(forceUpdate);
        getObj(MeshLibrary).layerChanged(LayerGroups::GroupScratch, -1);

        globalUpdate ? doUpdate(SourceMorph) : doUpdate(SourceScratch);
    } else if (button == &configIcon) {
        int envEnum = getSetting(CurrentEnvGroup);

        MeshLibrary::EnvProps* props = getObj(MeshLibrary).getCurrentEnvProps(envEnum);

        bool isPlugin = true; //formatSplit(false, true);

        PopupMenu menu;

        if (envEnum != LayerGroups::GroupVolume) {
            menu.addItem(CfgDynamic, "Dynamic while live", true, props->dynamic);
        } else {
            menu.addItem(CfgLogarithmic, "Logarithmic", true, props->logarithmic);
        }

        if (envEnum == LayerGroups::GroupScratch) {
            menu.addItem(CfgGlobal, "Globally triggered", envEnum == LayerGroups::GroupScratch, props->global);
        }

        if (isPlugin) {
            menu.addItem(CfgSyncTempo, "Sync to host tempo", isPlugin, props->tempoSync);
        }

        PopupMenu scaleMenu;
        scaleMenu.addItem(CfgScale1_16x, "1/16x", true, props->scale == -16);
        scaleMenu.addItem(CfgScale1_4x, "1/4x", true, props->scale == -4);
        scaleMenu.addItem(CfgScale1_2x, "1/2x", true, props->scale == -2);
        scaleMenu.addItem(CfgScale1x, "1x", true, props->scale == 1);
        scaleMenu.addItem(CfgScale2x, "2x", true, props->scale == 2);
        scaleMenu.addItem(CfgScale4x, "4x", true, props->scale == 4);
        scaleMenu.addItem(CfgScale16x, "16x", true, props->scale == 16);

        menu.addSubMenu("Duration scale", scaleMenu, true);

        auto options = PopupMenu::Options()
            .withTargetScreenArea(configIcon.getScreenBounds())
            .withItemThatMustBeVisible(0)
            .withMinimumNumColumns(1)
            .withMinimumWidth(100)
            .withStandardItemHeight(22);

        menu.showMenuAsync(options, [this, props](int result) {
            chooseConfigScale(result, props);
        });
    } else if (button == &enableButton) {
        if (getEnvRasterizer()) {
            int envEnum = getSetting(CurrentEnvGroup);
            MeshLibrary::EnvProps* envProps = getObj(MeshLibrary).getCurrentEnvProps(envEnum);

            bool isNowEnabled = !envProps->active;

            enableButton.setHighlit(isNowEnabled);
            envProps->active = isNowEnabled;

            switch (envEnum) {
                case LayerGroups::GroupVolume:
                    volumeIcon.setPowered(isNowEnabled);
                    break;
                case LayerGroups::GroupPitch:
                    pitchIcon.setPowered(isNowEnabled);
                    break;
                case LayerGroups::GroupScratch: {
                    int scratchChan = getObj(MeshLibrary).getCurrentIndex(LayerGroups::GroupScratch);
                    scratchIcon.setPowered(isNowEnabled);

                    MeshLibrary::Properties* currTimeProps = getObj(MeshLibrary).getCurrentProps(
                            LayerGroups::GroupTime);
                    MeshLibrary::Properties* currFreqProps = getObj(MeshLibrary).getCurrentProps(
                            LayerGroups::GroupSpect);

                    if (currTimeProps != nullptr && currTimeProps->scratchChan == scratchChan) {
                        getObj(Waveform3D).setSpeedApplicable(isNowEnabled);
                    }

                    if (currFreqProps != nullptr && currFreqProps->scratchChan == scratchChan) {
                        getObj(Spectrum3D).setSpeedApplicable(isNowEnabled);
                    }

                    break;
                }
                default:
                    break;
            }

            // don't use 'postupdatemessage' because it checks if mesh is active
            // so won't remove the effect of the envelope when disabled

            triggerRefreshUpdate();
        }
    } else if (button == &loopIcon || button == &sustainIcon) {
        toggleEnvelopePoint(button);
    }
}

void EnvelopeInter2D::toggleEnvelopePoint(Button* button) {
    bool isLoop = button == &loopIcon;

    vector < Vertex * > &selected = getSelected();

    if (selected.size() != 1) {
        showConsoleMsg("Select a single vertex");
    } else {
        EnvelopeMesh* currentMesh = getCurrentMesh();

        if (currentMesh == nullptr) {
            return;
        }

        bool onlyShallowUpdate = true;

        if (!currentMesh) {
            jassertfalse;
        } else {
            if (getSetting(DrawWave)) {
                showConsoleMsg("Cannot set loop in wave draw mode");
            } else {
                IconButton &bttn = isLoop ? loopIcon : sustainIcon;
                Vertex* vert = selected.front();
                TrilinearCube* cube = nullptr;

                const vector <Intercept> &controlPoints = rasterizer->getRastData().intercepts;
                for (const auto& icpt : controlPoints) {
                    for (int j = 0; j < vert->getNumOwners(); ++j) {
                        TrilinearCube* TrilinearCube = vert->owners[j];

                        if (icpt.cube == TrilinearCube && TrilinearCube != nullptr) {
                            cube = TrilinearCube;
                        }
                        break;
                    }
                }

                jassert(cube != nullptr);

                if (cube == nullptr) {
                    return;
                }

                set < TrilinearCube * > &envLines = isLoop ? currentMesh->loopCubes : currentMesh->sustainCubes;

                bool wasAlreadySet = envLines.find(cube) != envLines.end();
                bool didAnything = false;

                removeCurrentEnvLine(isLoop);

                if (!wasAlreadySet) {
                    envLines.insert(cube);
                }

                getEnvRasterizer()->evaluateLoopSustainIndices();
                didAnything |= synchronizeEnvPoints(vert, true);

                bool isNowSet = envLines.find(cube) != envLines.end();
                didAnything |= isNowSet != wasAlreadySet;

                onlyShallowUpdate = !didAnything;
                bttn.setHighlit(isNowSet);

                if (onlyShallowUpdate) {
                    performUpdate(Update);
                } else {
                    getObj(EditWatcher).setHaveEditedWithoutUndo(true);
                    flag(DidMeshChange) = true;

                    refresh();
                }
            }
        }
    }
}

void EnvelopeInter2D::switchedEnvelope(int envEnum, bool performUpdate, bool force) {
    progressMark

    int oldEnvEnum = getSetting(CurrentEnvGroup);

    if (!force && envEnum == oldEnvEnum) {
        return;
    }

    getSetting(CurrentEnvGroup) = envEnum;
    bool changedToOrFromVol = (envEnum == LayerGroups::GroupVolume) != (oldEnvEnum == LayerGroups::GroupVolume);

    MeshLibrary::EnvProps &props = *getObj(MeshLibrary).getCurrentEnvProps(envEnum);

    envPanel->backgroundTempoSynced = props.tempoSync;

    updateHighlights();

    OldMeshRasterizer* rast = getRast(envEnum);
    setRasterizer(rast);

    if (getSetting(CurrentMorphAxis) == Vertex::Time && changedToOrFromVol) {
        envPanel->updateBackground(false);
    }

    if (envEnum == LayerGroups::GroupWavePitch) {
        if (PitchedSample* current = getObj(Multisample).getCurrentSample()) {
            rast->setMesh(current->mesh.get());
        }
    } else if (envEnum == LayerGroups::GroupPitch) {
        if (EnvRasterizer* envRast = getEnvRasterizer()) {
            envRast->setMesh(getObj(MeshLibrary).getCurrentEnvMesh(LayerGroups::GroupPitch));
        }
    }

    enableButton.setHighlit(isCurrentMeshActive());

    if ((oldEnvEnum == LayerGroups::GroupScratch) != (envEnum == LayerGroups::GroupScratch)) {
        if (envEnum == LayerGroups::GroupScratch) {
            envPanel->setSelectorVisibility(true);
        } else {
            envPanel->setSelectorVisibility(false);
        }
    }

    state.reset();
    clearSelectedAndCurrent();

    configIcon.setPowered(props.dynamic || props.global || props.scale != 1 || props.tempoSync);

    layerType = envEnum;
    envPanel->curveIsBipolar = envEnum == LayerGroups::GroupPitch || envEnum == LayerGroups::GroupWavePitch;
    vertsAreWaveApplicable = envEnum == LayerGroups::GroupWavePitch;

    delegateUpdate(performUpdate);
}

void EnvelopeInter2D::adjustAddedLine(TrilinearCube* addedLine) {
    TrilinearCube::Face lowFace = addedLine->getFace(Vertex::Time, TrilinearCube::LowPole);
    TrilinearCube::Face highFace = addedLine->getFace(Vertex::Time, TrilinearCube::HighPole);

    for (int i = 0; i < 4; ++i) {
        lowFace[i]->values[Vertex::Time] = 0.f;
        highFace[i]->values[Vertex::Time] = 1.f;
    }
}

void EnvelopeInter2D::doSustainReleaseChange(bool isSustain) {
}

void EnvelopeInter2D::performUpdate(UpdateType updateType) {
    Interactor::performUpdate(updateType);
}

void EnvelopeInter2D::doExtraMouseDrag(const MouseEvent &e) {
    Interactor2D::doExtraMouseDrag(e);

    if (actionIs(DraggingVertex) || actionIs(ReshapingCurve) || actionIs(DraggingCorner)) {
        if (getSetting(CurrentEnvGroup) == LayerGroups::GroupWavePitch && getSetting(WaveLoaded)) {
            if (PitchedSample* sample = getObj(Multisample).getCurrentSample()) {
                sample->createPeriodsFromEnv(&getObj(EnvPitchRast));
            }
        }

        syncEnvPointsImplicit();
    }
}

int EnvelopeInter2D::getUpdateSource() {
    if (getSetting(CurrentEnvGroup) == LayerGroups::GroupScratch) {
        return UpdateSources::SourceScratch;
    }

    return updateSource;
}

Mesh* EnvelopeInter2D::getMesh() {
    if (auto* envRast = dynamic_cast<EnvRasterizer*>(getRasterizer())) {
        return envRast->getCurrentMesh();
    }

    if (PitchedSample* current = getObj(Multisample).getCurrentSample()) {
        return current->mesh.get();
    }

    jassertfalse;
    return &backupMesh;
}

void EnvelopeInter2D::setEnabledHighlight(bool highlit) {
    enableButton.setHighlit(highlit);
}

void EnvelopeInter2D::updateHighlights() {
    int envMesh = getSetting(CurrentEnvGroup);
    enableButton.setHighlit(isCurrentMeshActive());

    volumeIcon.setHighlit(envMesh == LayerGroups::GroupVolume);
    pitchIcon.setHighlit(envMesh == LayerGroups::GroupPitch);
    wavePitchIcon.setHighlit(envMesh == LayerGroups::GroupWavePitch);
    scratchIcon.setHighlit(envMesh == LayerGroups::GroupScratch);
    wavePitchIcon.setApplicable(getSetting(WaveLoaded) == 1);

    jassert(!(wavePitchIcon.isHighlit() && !wavePitchIcon.isApplicable()));
}

void EnvelopeInter2D::enterClientLock() {
    getObj(SynthAudioSource).getLock().enter();
    panel->getRenderLock().enter();
}

void EnvelopeInter2D::exitClientLock() {
    panel->getRenderLock().exit();
    getObj(SynthAudioSource).getLock().exit();
}

bool EnvelopeInter2D::doesMeshChangeWarrantGlobalUpdate() {
    return Interactor::doesMeshChangeWarrantGlobalUpdate();
}

bool EnvelopeInter2D::isCurrentMeshActive() {
    int envType = getSetting(CurrentEnvGroup);
    if (envType != LayerGroups::GroupWavePitch) {
        MeshLibrary::Properties* props = getObj(MeshLibrary).getCurrentEnvProps(envType);
        return props->active;
    }

    return getObj(Multisample).getCurrentSample() != nullptr;
}

void EnvelopeInter2D::validateMesh() {
    Interactor::validateMesh();

//	EnvRasterizer* envRast = static_cast<EnvRasterizer*>(getRasterizer());
//	EnvelopeMesh* envMesh = getCurrentMesh();

//	vector<TrilinearCube*>& lines = envMesh->lines;

// todo
//	bool isContained = false;
//	for(int i = 0; i < (int) lines.size(); ++i)
//	{
//		if(lines[i] == envMesh->loopLine)
//		{
//			isContained = true;
//		}
//	}
//
//	if(isContained)
//	{
//		// if a line has been deleted, the intercepts won't be updated at this point
//		envRast->calcIntercepts();
//		envRast->evaluateLoopSustainIndices();
//
//		const vector<Intercept>& controlPoints = envRast->getIntercepts();
//		for(int i = 0; i < (int) controlPoints.size(); ++i)
//		{
//			if(envMesh->loopLines.find(controlPoints[i].cube) != envMesh->loopLines.end())
//			{
//				if(i > (controlPoints.size() - 1) - EnvRasterizer::loopMinSizeIcpts)
//				{
//					isContained = false;
//				}
//			}
//		}
//	}
//
//	if(! isContained)
//	{
//		envMesh->loopLine = nullptr;
//		calcSustainLoopIndices();
//	}
}

String EnvelopeInter2D::getDefaultFolder() {
    int mesh = getSetting(CurrentEnvGroup);
    switch (mesh) {
        case LayerGroups::GroupVolume:  return "volume";
        case LayerGroups::GroupPitch:   return "pitch";
        case LayerGroups::GroupScratch: return "scratch";
        default:
            break;
    }

    return {};
}

Range<float> EnvelopeInter2D::getVertexPhaseLimits(Vertex* vert) {
    // we want to disallow moving verts that are NOT after the sustain line
    // (i.e. they are not part of the release curve) beyond the boundary of 1

    Range<float> range = vertexLimits[Vertex::Phase];
    Range<float> minRange = Interactor2D::getVertexPhaseLimits(vert);

    if (EnvRasterizer* envRast = getEnvRasterizer()) {
        const vector <Intercept>& controlPoints = envRast->getRastData().intercepts;

        if (controlPoints.empty() || vert == nullptr) {
            return Interactor2D::getVertexPhaseLimits(vert);
        }

        int icptIndex = -1;
        int sustIdx, loopIdx;
        envRast->getIndices(loopIdx, sustIdx);

        for (int i = 0; i < (int) controlPoints.size(); ++i) {
            if (vert->isOwnedBy(controlPoints[i].cube)) {
                icptIndex = i;
            }
        }

        if (icptIndex >= 0) {
            if (icptIndex == sustIdx) {
                range.setEnd(1.f);
            }

            if (icptIndex == loopIdx && sustIdx > 0) {
                range.setEnd(controlPoints[sustIdx].x - 0.001f);
            }
        }
    }

    return range.getIntersectionWith(minRange);
}

void EnvelopeInter2D::waveOverlayChanged() {
    progressMark

    bool drawWave = getSetting(DrawWave) == 1;

    if (drawWave) {
        switchedEnvelope(LayerGroups::GroupWavePitch, true, true);
    } else {
        if (getSetting(CurrentEnvGroup) == LayerGroups::GroupWavePitch)
            switchedEnvelope(LayerGroups::GroupPitch, true);
        else {
            getObj(EnvPitchRast).setMesh(getObj(MeshLibrary).getCurrentEnvMesh(LayerGroups::GroupPitch));
        }
    }

    updateHighlights();
}

void EnvelopeInter2D::updatePhaseLimit(float limit) {
    limit = jmax(1.f, limit);
    float oldZoomLimit = envPanel->getZoomPanel()->rect.xMaximum;

    if (oldZoomLimit != limit) {
        envPanel->getZoomPanel()->panelZoomChanged(false);
        envPanel->setZoomLimit(limit);
        envPanel->repaint();
    }
}

EnvRasterizer* EnvelopeInter2D::getEnvRasterizer() {
    return dynamic_cast<EnvRasterizer*>(rasterizer);
}

void EnvelopeInter2D::transferLineProperties(TrilinearCube* from, TrilinearCube* to1, TrilinearCube* to2) {
    Interactor::transferLineProperties(from, to1, to2);

    if (EnvelopeMesh* envMesh = getCurrentMesh()) {
        set < TrilinearCube * > &loopLines = envMesh->loopCubes;
        set < TrilinearCube * > &sustLines = envMesh->sustainCubes;

        bool wasLoopLine = (loopLines.find(from) != loopLines.end());
        bool wasSustLine = (sustLines.find(from) != sustLines.end());

        if (wasLoopLine) {
            loopLines.erase(from);
            loopLines.insert(to1);
            loopLines.insert(to2);
        }

        if (wasSustLine) {
            sustLines.erase(from);
            sustLines.insert(to1);
            sustLines.insert(to2);
        }
    }
}

void EnvelopeInter2D::removeCurrentEnvLine(bool isLoop) {
    const vector <Intercept> &controlPoints = rasterizer->getRastData().intercepts;
    EnvelopeMesh* envMesh = getCurrentMesh();

    if (envMesh == nullptr) {
        return;
    }

    set<TrilinearCube*>& loopLines = envMesh->loopCubes;
    set<TrilinearCube*>& sustLines = envMesh->sustainCubes;

    for (const auto& icpt : controlPoints) {
        TrilinearCube* cube = icpt.cube;

        if (isLoop && loopLines.find(cube) != loopLines.end()) {
            loopLines.erase(cube);
        }

        if (!isLoop && sustLines.find(cube) != sustLines.end()) {
            sustLines.erase(cube);
        }
    }
}

void EnvelopeInter2D::syncEnvPointsImplicit() {
    EnvRasterizer* envRast = getEnvRasterizer();

    if (envRast == nullptr) {
        return;
    }

    const EnvelopeMesh* envMesh = envRast->getEnvMesh();

    if (envMesh != nullptr) {
        bool areMovingSustainLine = false, areMovingLoopLine = false;
        vector < Vertex * > &selected = getSelected();
        Vertex* vertToSyncFrom = state.currentVertex;

        const set<TrilinearCube*> &loopLines = envMesh->loopCubes;
        const set<TrilinearCube*> &sustLines = envMesh->sustainCubes;

        if (!selected.empty()) {
            for (auto vert : selected) {
                for (int j = 0; j < vert->getNumOwners(); ++j) {
                    TrilinearCube* TrilinearCube = vert->owners[j];

                    if (TrilinearCube != nullptr && loopLines.find(TrilinearCube) != loopLines.end()) {
                        areMovingLoopLine = true;
                        vertToSyncFrom = vert;
                    }
                    break;
                }

                for (int j = 0; j < vert->getNumOwners(); ++j) {
                    TrilinearCube* TrilinearCube = vert->owners[j];

                    if (TrilinearCube != nullptr && sustLines.find(TrilinearCube) != sustLines.end()) {
                        areMovingSustainLine = true;
                        vertToSyncFrom = vert;
                    }
                    break;
                }
            }
        } else if (state.currentCube != nullptr) {
            areMovingLoopLine = (loopLines.find(state.currentCube) != loopLines.end());
            areMovingSustainLine = (sustLines.find(state.currentCube) != sustLines.end());
        }

        jassert(!areMovingLoopLine || !areMovingSustainLine);

        if (vertToSyncFrom != nullptr && areMovingSustainLine || areMovingLoopLine) {
            synchronizeEnvPoints(vertToSyncFrom, areMovingLoopLine);
        }
    }
}

void EnvelopeInter2D::layerChanged() {
    state.reset();
    clearSelectedAndCurrent();

    int envEnum = getSetting(CurrentEnvGroup);
    getRast(envEnum)->setMesh(getObj(MeshLibrary).getCurrentEnvMesh(envEnum));
    getObj(VertexPropertiesPanel).updateSliderValues(true);

    enableButton.setHighlit(isCurrentMeshActive());
    envPanel->pendingScaleUpdate = true;

    delegateUpdate(true);
}

int EnvelopeInter2D::getLayerType() {
    return getSetting(CurrentEnvGroup);
}

void EnvelopeInter2D::chooseConfigScale(int result, MeshLibrary::EnvProps* props) {
    if (result == 0) {
        return;
    }

    bool editChange = false;

    if (result >= CfgScale1_16x && result <= CfgScale16x) {
        int newScale = -1;

        switch (result) {
            case CfgScale1_16x: newScale = -16; break;
            case CfgScale1_4x:  newScale = -4;  break;
            case CfgScale1_2x:  newScale = -2;  break;
            case CfgScale1x:    newScale = 1;   break;
            case CfgScale2x:    newScale = 2;   break;
            case CfgScale4x:    newScale = 4;   break;
            case CfgScale16x:   newScale = 16;  break;
            default:
                break;
        }

        if (Util::assignAndWereDifferent(props->scale, newScale)) {
            editChange = true;
            doGlobalUIUpdate(true);
        }
    } else {
        switch (result) {
            case CfgDynamic:
                props->dynamic = !props->dynamic;
                break;

            case CfgSyncTempo:
                props->tempoSync = !props->tempoSync;
                envPanel->backgroundTempoSynced = props->tempoSync;

                break;

            case CfgGlobal: {
                props->global = !props->global;
                getObj(SynthAudioSource).setPendingGlobalChange();
                break;
            }

            case CfgLogarithmic:
                props->logarithmic = !props->logarithmic;
                envPanel->updateBackground(true);
                triggerRefreshUpdate();
                break;

            default:
                break;
        }

        editChange = true;
        configIcon.setPowered(props->isOperating());
    }
    if (editChange) {
        getObj(EditWatcher).setHaveEditedWithoutUndo(true);
    }
}

void EnvelopeInter2D::delegateUpdate(bool shouldDoUpdate) {
    if (getSetting(CurrentMorphAxis) != Vertex::Time) {
        if (shouldDoUpdate) {
            doUpdate(SourceEnvelope3D);
        }
    } else {
        if (shouldDoUpdate) {
            jassert(rasterizer == getRasterizer());

            if (EnvRasterizer* envRast = getEnvRasterizer()) {
                envRast->setWantOneSamplePerCycle(false);
            }

            rasterizer->performUpdate(Update);
            performUpdate(Update);
        }

        envPanel->zoomAndRepaint();
    }
}

void EnvelopeInter2D::reset() {
    layerSelector.reset();
}

void EnvelopeInter2D::enablementsChanged() {
    auto& meshLib = getObj(MeshLibrary);

    bool anyScratchActive = false;
    bool anyVolumeActive = false;
    bool anyPitchActive = false;

    MeshLibrary::LayerGroup& scratchGroup = meshLib.getLayerGroup(LayerGroups::GroupScratch);
    MeshLibrary::LayerGroup& volumeGroup = meshLib.getLayerGroup(LayerGroups::GroupVolume);
    MeshLibrary::LayerGroup& pitchGroup = meshLib.getLayerGroup(LayerGroups::GroupPitch);

    for (int i = 0; i < (int) scratchGroup.layers.size(); ++i) {
        anyScratchActive |= meshLib.getCurrentProps(i)->active;
    }

    for (int i = 0; i < (int) volumeGroup.layers.size(); ++i) {
        anyVolumeActive |= meshLib.getCurrentProps(i)->active;
    }

    for (int i = 0; i < (int) pitchGroup.layers.size(); ++i) {
        anyPitchActive |= meshLib.getCurrentProps(i)->active;
    }

    scratchIcon.setPowered(anyScratchActive);
    volumeIcon.setPowered(anyVolumeActive);
    pitchIcon.setPowered(anyPitchActive);

    MeshLibrary::EnvProps* props = meshLib.getCurrentEnvProps(getSetting(CurrentEnvGroup));

    if (props != nullptr)
        configIcon.setPowered(props->dynamic || props->global || props->scale != 1 || props->tempoSync);
}


void EnvelopeInter2D::triggerButton(int id) {
    switch (id) {
        case CycleTour::IdBttnAdd:     buttonClicked(&addRemover.add);    break;
        case CycleTour::IdBttnRemove:  buttonClicked(&addRemover.remove); break;
        case CycleTour::IdBttnEnable:  buttonClicked(&enableButton);      break;
        case CycleTour::IdBttnLoop:    buttonClicked(&loopIcon);          break;
        case CycleTour::IdBttnSustain: buttonClicked(&sustainIcon);       break;
        default:
            break;
    }
}

OldMeshRasterizer* EnvelopeInter2D::getRast(int envEnum) {
    switch (envEnum) {
        case LayerGroups::GroupVolume:    return &getObj(EnvVolumeRast);
        case LayerGroups::GroupPitch:     return &getObj(EnvPitchRast);
        case LayerGroups::GroupScratch:   return &getObj(EnvScratchRast);
        case LayerGroups::GroupWavePitch: return &getObj(EnvWavePitchRast);
        default:
            break;
    }

    return nullptr;
}

bool EnvelopeInter2D::shouldDoDimensionCheck() {
    if (getSetting(CurrentEnvGroup) == LayerGroups::GroupWavePitch) {
        return false;
    }

    return Interactor::shouldDoDimensionCheck();
}

bool EnvelopeInter2D::addNewCube(float startTime, float x, float y, float curve) {
    if (getSetting(CurrentEnvGroup) != LayerGroups::GroupWavePitch) {
        return Interactor::addNewCube(startTime, x, y, curve);
    }

    Interactor2D::doCreateVertex();

    return true;
}

vector<TrilinearCube*> EnvelopeInter2D::getLinesToSlideOnSingleSelect() {
    vector <TrilinearCube*> cubes;
    const vector <Intercept>& controlPoints = rasterizer->getRastData().intercepts;

    if (EnvRasterizer* envRast = getEnvRasterizer()) {
        if (!controlPoints.empty()) {
            int icptIndex = -1;
            int sustIdx, loopIdx;
            envRast->getIndices(loopIdx, sustIdx);

            if (sustIdx < 0 && loopIdx < 0) {
                return cubes;
            }

            TrilinearCube* lastCube = nullptr;
            bool startMoving = false;

            if (state.currentCube != nullptr) {
                bool releaseIsRightmost = controlPoints[sustIdx].x >= 1.f;

                for (int i = 0; i < (int) controlPoints.size(); ++i) {
                    const Intercept& icpt = controlPoints[i];

                    if (startMoving && icpt.cube != nullptr && icpt.cube != lastCube) {
                        cubes.push_back(icpt.cube);
                        lastCube = icpt.cube;
                    }

                    if (icpt.cube == state.currentCube) {
                        if (i == sustIdx || i == loopIdx) {
                            startMoving = true;
                        } else {
                            break;
                        }
                    }
                }
            }
        }
    }

    return cubes;
}


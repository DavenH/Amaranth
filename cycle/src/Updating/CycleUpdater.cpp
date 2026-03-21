#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Audio/Multisample.h>
#include <Definitions.h>
#include <Inter/Interactor.h>

#include "CycleUpdater.h"
#include "EnvelopeDelegate.h"
#include "MorphUpdate.h"
#include "ScratchUpdate.h"
#include "SpectDelegate.h"

#include "../App/Initializer.h"
#include "../Curve/E3Rasterizer.h"
#include "../Curve/GraphicRasterizer.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../Inter/EnvelopeInter3D.h"
#include "../Inter/SpectrumInter2D.h"
#include "../Inter/SpectrumInter3D.h"
#include "../Inter/WaveformInter2D.h"
#include "../Inter/WaveformInter3D.h"
#include "../UI/Effects/EqualizerUI.h"
#include "../UI/Effects/IrModellerUI.h"
#include "../UI/Effects/WaveshaperUI.h"
#include "../UI/Panels/DerivativePanel.h"
#include "../UI/VertexPanels/GuideCurvePanel.h"
#include "../UI/VisualDsp.h"
#include "../Util/CycleEnums.h"

CycleUpdater::CycleUpdater(SingletonRepo* repo) :
        SingletonAccessor(repo, "CycleUpdater")
    ,   graphCreated(false)
    , 	lastViewStage(ViewStages::PreProcessing) {
}

void CycleUpdater::init() {
    updater = &getObj(Updater);

    if (graphCreated) {
        return;
    }

    envelopeDelegate = std::make_unique<EnvelopeDelegate>(repo);
    spectDelegate    = std::make_unique<SpectDelegate>(repo);
    morphUpdate      = std::make_unique<MorphUpdate>(repo);
    scratchUpdate    = std::make_unique<ScratchUpdate>(repo);

    createUpdateGraph();
    graphCreated = true;
}

void CycleUpdater::finishInitialization() {
    if (!graphCreated) {
        init();
    }

    viewStageChanged(true);
}

CycleUpdater::Node* CycleUpdater::createNode() {
    return ownedNodes.add(new Node());
}

CycleUpdater::Node* CycleUpdater::createNode(Updateable* updateable) {
    return ownedNodes.add(new Node(updateable));
}

void CycleUpdater::createUpdateGraph() {
    time2Itr 	= createNode(&getObj(WaveformInter2D));
    time3Itr 	= createNode(&getObj(WaveformInter3D));
    spect2Itr 	= createNode(&getObj(SpectrumInter2D));
    spect3Itr 	= createNode(&getObj(SpectrumInter3D));
    env2Itr 	= createNode(&getObj(EnvelopeInter2D));
    env3Itr 	= createNode(&getObj(EnvelopeInter3D));
    irModelItr 	= createNode(&getObj(IrModellerUI));
    wshpItr 	= createNode(&getObj(WaveshaperUI));
    eqlzerUI 	= createNode(&getObj(EqualizerUI));
    derivUI 	= createNode(&getObj(DerivativePanel));
    guideCurveItr 	= createNode(&getObj(GuideCurvePanel));

    time2Rast  	= createNode(&getObj(TimeRasterizer));
    guideCurveRast = createNode(getObj(GuideCurvePanel).getRasterizer());
    irModelRast = createNode(getObj(IrModellerUI).getRasterizer());
    wshpRast 	= createNode(getObj(WaveshaperUI).getRasterizer());

    envProc 	= createNode(getObj(VisualDsp).getEnvProcessor());
    timeProc  	= createNode(getObj(VisualDsp).getTimeProcessor());
    spectProc 	= createNode(getObj(VisualDsp).getFFTProcessor());
    effectsProc = createNode(getObj(VisualDsp).getFXProcessor());

    univNode 	= createNode();
    allButFX 	= createNode();
    wshpDsp 	= createNode();
    irModelDsp 	= createNode();

    unisonItr 	= createNode();
    unison		= createNode();
    ctrlNode	= createNode();

    envDlg		= createNode(envelopeDelegate.get());
    spectDlg	= createNode(spectDelegate.get());
    scratchRast	= createNode(scratchUpdate.get());
    morphNode   = createNode(morphUpdate.get());
    synthNode   = createNode();

    timeUIs 	= createNode();
    spectUIs 	= createNode();

    Node* timeNodes[] = { timeProc, time2Rast, synthNode };

    spect2Itr	->marks(spectDlg);
    spect3Itr	->marks(spectDlg);
    spect3Itr	->marks(synthNode);
    time2Itr	->marks(Array<Node*>(timeNodes, 3));
    time3Itr	->marks(Array<Node*>(timeNodes, 3));
    guideCurveItr->marks(synthNode);
    scratchRast	->marks(synthNode);
    scratchRast	->marks(scratchRast);
    morphNode	->marks(morphNode);
    ctrlNode	->marks(envProc);
    eqlzerUI	->marks(effectsProc);
    env2Itr		->marks(envDlg);
    env3Itr		->marks(envDlg);

    Node* universalNodes[] 	= { morphNode, spectDlg, envDlg, guideCurveRast, wshpRast, irModelRast,
                                scratchRast, irModelDsp, unison, timeProc, time2Rast, synthNode };

    Node* allButFxNodes[] 	= { morphNode, spectDlg, envDlg, guideCurveRast, scratchRast, timeProc,
                                time2Rast, synthNode };

    univNode	->marks(Array<Node*>(universalNodes, numElementsInArray(universalNodes)));
    allButFX	->marks(Array<Node*>(allButFxNodes, numElementsInArray(allButFxNodes)));

    spectDlg	->updatesAfter(scratchRast);
    time2Rast	->updatesAfter(scratchRast);
    spect2Itr	->updatesAfter(spectUIs);
    spect3Itr	->updatesAfter(spectUIs);
    time2Itr	->updatesAfter(timeUIs);
    time3Itr	->updatesAfter(timeUIs);
    derivUI		->updatesAfter(timeUIs);
    timeProc	->updatesAfter(time2Rast);
    timeUIs		->updatesAfter(timeProc);
    spectProc	->updatesAfter(timeProc);
    spectUIs	->updatesAfter(spectProc);
    synthNode	->updatesAfter(spectProc);
    spectProc	->updatesAfter(spectDlg);
    time2Rast	->updatesAfter(morphNode);
    spectDlg	->updatesAfter(morphNode);
    envDlg		->updatesAfter(morphNode);

    Node* startingNodes[]	= { morphNode, time2Rast, envDlg, irModelDsp, wshpDsp, effectsProc, timeProc,
                                unisonItr, synthNode, irModelRast, wshpRast, guideCurveRast, scratchRast, spectDlg };

    updater->getGraph().addHeadNodes(Array<Node*>(startingNodes, numElementsInArray(startingNodes)));

    // TODO envelopevisibilitychange

    irModelItr	->marksAndUpdatesAfter(irModelRast);
    irModelItr	->marksAndUpdatesAfter(irModelDsp);
    wshpItr		->marksAndUpdatesAfter(wshpDsp);
    wshpItr		->marksAndUpdatesAfter(wshpRast);
    guideCurveItr->marksAndUpdatesAfter(guideCurveRast);
    unisonItr	->marksAndUpdatesAfter(unison);

    envelopeVisibilityChanged();

    updater->setStartingNode(UpdateSources::SourceAll, 			univNode);
    updater->setStartingNode(UpdateSources::SourceAllButFX, 	allButFX);
    updater->setStartingNode(UpdateSources::SourceGuideCurve, 	guideCurveItr);
    updater->setStartingNode(UpdateSources::SourceEnvelope2D, 	env2Itr);
    updater->setStartingNode(UpdateSources::SourceEnvelope3D, 	env3Itr);
    updater->setStartingNode(UpdateSources::SourceEqualizer, 	eqlzerUI);
    updater->setStartingNode(UpdateSources::SourceIrModeller, 	irModelItr);
    updater->setStartingNode(UpdateSources::SourceSpectrum2D, 	spect2Itr);
    updater->setStartingNode(UpdateSources::SourceSpectrum3D, 	spect3Itr);
    updater->setStartingNode(UpdateSources::SourceMorph, 		morphNode);
    updater->setStartingNode(UpdateSources::SourceOscCtrls, 	ctrlNode);
    updater->setStartingNode(UpdateSources::SourceScratch, 		scratchRast);
    updater->setStartingNode(UpdateSources::SourceUnison, 		unisonItr);
    updater->setStartingNode(UpdateSources::SourceWaveform2D, 	time2Itr);
    updater->setStartingNode(UpdateSources::SourceWaveform3D, 	time3Itr);
    updater->setStartingNode(UpdateSources::SourceWaveshaper, 	wshpItr);
}

void CycleUpdater::envelopeVisibilityChanged() {
    if (getSetting(CurrentMorphAxis) == Vertex::Time) {
        env2Itr->updatesAfter(envDlg);
        env3Itr->doesntUpdateAfter(envDlg);
    } else {
        env2Itr->doesntUpdateAfter(envDlg);
        env3Itr->updatesAfter(envDlg);
    }
}

void CycleUpdater::moveTimeUIs(int viewStage, int lastViewStage) {
//	jassert(viewStage != lastViewStage);

    Node* fromNode = nullptr;
    Node* toNode = nullptr;

    switch (lastViewStage) {
        case ViewStages::PreProcessing: fromNode = timeProc;	break;
        case ViewStages::PostEnvelopes: fromNode = envProc; 	break;
        case ViewStages::PostSpectrum: 	fromNode = spectProc; 	break;
        case ViewStages::PostFX: 		fromNode = effectsProc; break;
        default: break;
    }

    switch (viewStage) {
        case ViewStages::PreProcessing: toNode = timeProc; 		break;
        case ViewStages::PostEnvelopes: toNode = envProc; 		break;
        case ViewStages::PostSpectrum: 	toNode = spectProc; 	break;
        case ViewStages::PostFX: 		toNode = effectsProc; 	break;
        default: break;
    }

    jassert(fromNode != nullptr);
    jassert(toNode != nullptr);

    timeUIs->doesntUpdateAfter(fromNode);
    timeUIs->updatesAfter(toNode);
}

void CycleUpdater::removeDspFXConnections() const {
    effectsProc	->doesntUpdateAfter(irModelRast);
    effectsProc	->doesntUpdateAfter(wshpRast);
    eqlzerUI	->doesntUpdateAfter(effectsProc);
    envProc		->doesntUpdateAfter(unison);
}

void CycleUpdater::setDspFXConnections() const {
    effectsProc	->updatesAfter(irModelRast);
    effectsProc	->updatesAfter(wshpRast);
    eqlzerUI	->updatesAfter(effectsProc);
    envProc		->updatesAfter(unison);
}

void CycleUpdater::setTimeFreqParents() {
    layerChanged(LayerGroups::GroupScratch, -1);
    layerChanged(LayerGroups::GroupGuideCurve, -1);
}

void CycleUpdater::setTimeFreqChildren(bool toFFT) const {
    if (toFFT) {
        spectProc->updatesAfter(timeProc);
        spectProc->updatesAfter(spectDlg);
    } else {
        envProc->updatesAfter(timeProc);
        envProc->updatesAfter(spectDlg);
    }
}

void CycleUpdater::removeTimeFreqChildren(bool fromFFT) const {
    if (fromFFT) {
        spectProc->doesntUpdateAfter(timeProc);
        spectProc->doesntUpdateAfter(spectDlg);
    } else {
        envProc->doesntUpdateAfter(timeProc);
        envProc->doesntUpdateAfter(spectDlg);
    }
}

void CycleUpdater::removeTimeFreqParents() const {
    timeProc->doesntUpdateAfter(scratchRast);
    spectDlg->doesntUpdateAfter(scratchRast);
}

void CycleUpdater::viewStageChanged(bool force) {
    int viewStage = getSetting(ViewStage);

    moveTimeUIs(viewStage, lastViewStage);

    if (force || (viewStage >= ViewStages::PostEnvelopes) != (lastViewStage >= ViewStages::PostEnvelopes)) {
        if (viewStage >= ViewStages::PostEnvelopes) {
            // due to osc controls
            updater->getGraph().addHeadNode(envProc);
            setTimeFreqParents();
            setTimeFreqChildren(false);
            removeTimeFreqChildren(true);

            spectProc->updatesAfter(envProc);
            envProc->updatesAfter(envDlg);
        } else {
            updater->getGraph().removeHeadNode(envProc);
            setTimeFreqChildren(true);
            removeTimeFreqParents();
            removeTimeFreqChildren(false);

            spectProc->doesntUpdateAfter(envProc);
            envProc->doesntUpdateAfter(envDlg);
        }
    }

    if (force || (viewStage >= ViewStages::PostFX) != (lastViewStage >= ViewStages::PostFX)) {
        if (viewStage >= ViewStages::PostFX) {
            envProc		->updatesAfter	(unison);
            spectUIs	->updatesAfter	(effectsProc);
            spectUIs	->doesntUpdateAfter	(spectProc);
            effectsProc	->updatesAfter	(spectProc);
            effectsProc	->updatesAfter	(irModelRast);
            effectsProc	->updatesAfter	(wshpRast);

            setDspFXConnections();
        } else {
            envProc	->doesntUpdateAfter		(unison);
            spectUIs->updatesAfter			(spectProc);
            spectUIs->doesntUpdateAfter		(effectsProc);
            effectsProc	->doesntUpdateAfter	(spectProc);
            effectsProc	->doesntUpdateAfter	(irModelRast);
            effectsProc	->doesntUpdateAfter	(wshpRast);

            removeDspFXConnections();
        }
    }

    lastViewStage = viewStage;
}

void CycleUpdater::layerChanged(int layerGroup, int index) {
    Node* srcNode = layerGroup == LayerGroups::GroupScratch ? scratchRast : guideCurveRast;

    Array<int> types = getObj(MeshLibrary).getMeshTypesAffectedByCurrent(layerGroup);
    refreshConnections(srcNode, types);
}

void CycleUpdater::refreshConnections(Node* destNode, const Array<int>& meshTypes) {
    /// xxx is it timeProc or timeRast ?? these ought to be symmetrical...
    time2Rast	->doesntUpdateAfter(destNode);
    spectDlg	->doesntUpdateAfter(destNode);
    envDlg		->doesntUpdateAfter(destNode);
    scratchRast	->doesntUpdateAfter(destNode);

    if(getSetting(ViewStage) < ViewStages::PostEnvelopes) {
        return;
    }

    using namespace LayerGroups;

    for (int meshType : meshTypes) {
        switch (meshType) {
            case GroupTime:
                time2Rast->updatesAfter(destNode);
                break;

            case GroupSpect:
            case GroupPhase:
                spectDlg->updatesAfter(destNode);
                break;

            case GroupVolume:
            case GroupPitch:
            case GroupWavePitch:
            case GroupScratch:
                envDlg->updatesAfter(destNode);

                if(meshType == GroupScratch) {
                    scratchRast->updatesAfter(destNode);
                }
                break;
            default: break;
        }
    }
}


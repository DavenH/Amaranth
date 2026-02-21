#include <App/MeshLibrary.h>
#include <Definitions.h>
#include <Inter/Interactor.h>

#include "CycleUpdater.h"
#include "EnvelopeDelegate.h"
#include "MorphUpdate.h"
#include "ScratchUpdate.h"
#include "SpectDelegate.h"

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
#include "../UI/VertexPanels/DeformerPanel.h"
#include "../UI/VisualDsp.h"
#include "../Util/CycleEnums.h"

CycleUpdater::CycleUpdater(SingletonRepo* repo) :
        SingletonAccessor(repo, "CycleUpdater")
    , 	lastViewStage(ViewStages::PreProcessing) {
}

void CycleUpdater::init() {
    updater = &getObj(Updater);
}

void CycleUpdater::createUpdateGraph() {
    time2Itr 	= new Node(&getObj(WaveformInter2D));
    time3Itr 	= new Node(&getObj(WaveformInter3D));
    spect2Itr 	= new Node(&getObj(SpectrumInter2D));
    spect3Itr 	= new Node(&getObj(SpectrumInter3D));
    env2Itr 	= new Node(&getObj(EnvelopeInter2D));
    env3Itr 	= new Node(&getObj(EnvelopeInter3D));
    irModelItr 	= new Node(&getObj(IrModellerUI));
    wshpItr 	= new Node(&getObj(WaveshaperUI));
    eqlzerUI 	= new Node(&getObj(EqualizerUI));
    derivUI 	= new Node(&getObj(DerivativePanel));
    dfrmItr 	= new Node(&getObj(DeformerPanel));

    time2Rast  	= new Node(&getObj(TimeRasterizer));
    dfrmRast 	= new Node(getObj(DeformerPanel).getRasterizer());
    irModelRast = new Node(getObj(IrModellerUI).getRasterizer());
    wshpRast 	= new Node(getObj(WaveshaperUI).getRasterizer());

    envProc 	= new Node(getObj(VisualDsp).getEnvProcessor());
    timeProc  	= new Node(getObj(VisualDsp).getTimeProcessor());
    spectProc 	= new Node(getObj(VisualDsp).getFFTProcessor());
    effectsProc = new Node(getObj(VisualDsp).getFXProcessor());

    univNode 	= new Node();
    allButFX 	= new Node();
    wshpDsp 	= new Node();
    irModelDsp 	= new Node();

    unisonItr 	= new Node();
    unison		= new Node();
    ctrlNode	= new Node();

    envDlg		= new Node(envelopeDelegate.get());
    spectDlg	= new Node(spectDelegate.get());
    scratchRast	= new Node(scratchUpdate.get());
    morphNode   = new Node(morphUpdate.get());
    synthNode   = new Node();

    timeUIs 	= new Node();
    spectUIs 	= new Node();

    Node* timeNodes[] = { timeProc, time2Rast, synthNode };

    spect2Itr	->marks(spectDlg);
    spect3Itr	->marks(spectDlg);
    spect3Itr	->marks(synthNode);
    time2Itr	->marks(Array<Node*>(timeNodes, 3));
    time3Itr	->marks(Array<Node*>(timeNodes, 3));
    dfrmItr		->marks(synthNode);
    scratchRast	->marks(synthNode);
    scratchRast	->marks(scratchRast);
    morphNode	->marks(morphNode);
    ctrlNode	->marks(envProc);
    eqlzerUI	->marks(effectsProc);
    env2Itr		->marks(envDlg);
    env3Itr		->marks(envDlg);

    Node* universalNodes[] 	= { morphNode, spectDlg, envDlg, dfrmRast, wshpRast, irModelRast,
                                scratchRast, irModelDsp, unison, timeProc, time2Rast, synthNode };

    Node* allButFxNodes[] 	= { morphNode, spectDlg, envDlg, dfrmRast, scratchRast, timeProc,
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
                                unisonItr, synthNode, irModelRast, dfrmRast, scratchRast, spectDlg };

    updater->getGraph().addHeadNodes(Array<Node*>(startingNodes, numElementsInArray(startingNodes)));

    // TODO envelopevisibilitychange

    irModelItr	->marksAndUpdatesAfter(irModelRast);
    irModelItr	->marksAndUpdatesAfter(irModelDsp);
    wshpItr		->marksAndUpdatesAfter(wshpDsp);
    wshpItr		->marksAndUpdatesAfter(wshpRast);
    dfrmItr		->marksAndUpdatesAfter(dfrmRast);
    unisonItr	->marksAndUpdatesAfter(unison);

    envelopeVisibilityChanged();

    updater->setStartingNode(UpdateSources::SourceAll, 			univNode);
    updater->setStartingNode(UpdateSources::SourceAllButFX, 	allButFX);
    updater->setStartingNode(UpdateSources::SourceDeformer, 	dfrmItr);
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
    layerChanged(LayerGroups::GroupDeformer, -1);
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
    Node* srcNode = layerGroup == LayerGroups::GroupScratch ? scratchRast : dfrmRast;

    Array<int> types = getObj(MeshLibrary).getMeshTypesAffectedByCurrent(layerGroup);
    refreshConnections(srcNode, types);
}

void CycleUpdater::refreshConnections(Node* destNode, const Array<int>& meshTypes) {
    /// xxx is it timeProc or timeRast ?? these ought to be symmetrical...
    time2Rast	->doesntUpdateAfter(destNode);
    spectDlg	->doesntUpdateAfter(destNode);
    envDlg		->doesntUpdateAfter(destNode);
    scratchRast	->doesntUpdateAfter(destNode);

    if (getSetting(ViewStage) < ViewStages::PostEnvelopes) {
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

                if (meshType == GroupScratch) {
                    scratchRast->updatesAfter(destNode);
                }
                break;
            default: break;
        }
    }
}


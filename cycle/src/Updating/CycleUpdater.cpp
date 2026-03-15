#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Audio/Multisample.h>
#include <Definitions.h>
#include <Inter/Interactor.h>
#include <iostream>

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

namespace {
    void registerRepresentedTarget(std::map<Updateable*, CycleUpdater::Node*>& represented,
                                   Updateable* updateable,
                                   CycleUpdater::Node* node) {
        if (updateable == nullptr || node == nullptr) {
            return;
        }

        represented[updateable] = node;
    }

    String getUpdateableName(Updateable* updateable) {
        if (updateable == nullptr) {
            return "<null>";
        }

        if (auto* accessor = dynamic_cast<SingletonAccessor*>(updateable)) {
            return accessor->getName();
        }

        String updateName = updateable->getUpdateName();
        if (updateName.isNotEmpty()) {
            return updateName;
        }

        return "<unnamed>";
    }

    void traverseReachable(CycleUpdater::Node* node, std::set<CycleUpdater::Node*>& visited) {
        if (node == nullptr || visited.find(node) != visited.end()) {
            return;
        }

        visited.insert(node);

        for (auto* parent : node->getParents()) {
            traverseReachable(parent, visited);
        }

        for (auto* child : node->getChildren()) {
            traverseReachable(child, visited);
        }
    }
}

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
    validateUpdateGraph();
}

void CycleUpdater::createUpdateGraph() {
    time2Itr 	= new Node(updater.get(), &getObj(WaveformInter2D));
    time3Itr 	= new Node(updater.get(), &getObj(WaveformInter3D));
    spect2Itr 	= new Node(updater.get(), &getObj(SpectrumInter2D));
    spect3Itr 	= new Node(updater.get(), &getObj(SpectrumInter3D));
    env2Itr 	= new Node(updater.get(), &getObj(EnvelopeInter2D));
    env3Itr 	= new Node(updater.get(), &getObj(EnvelopeInter3D));
    irModelItr 	= new Node(updater.get(), &getObj(IrModellerUI));
    wshpItr 	= new Node(updater.get(), &getObj(WaveshaperUI));
    eqlzerUI 	= new Node(updater.get(), &getObj(EqualizerUI));
    derivUI 	= new Node(updater.get(), &getObj(DerivativePanel));
    guideCurveItr 	= new Node(updater.get(), &getObj(GuideCurvePanel));

    time2Rast  	= new Node(updater.get(), &getObj(TimeRasterizer));
    guideCurveRast = new Node(updater.get(), getObj(GuideCurvePanel).getRasterizer());
    irModelRast = new Node(updater.get(), getObj(IrModellerUI).getRasterizer());
    wshpRast 	= new Node(updater.get(), getObj(WaveshaperUI).getRasterizer());

    envProc 	= new Node(updater.get(), getObj(VisualDsp).getEnvProcessor());
    timeProc  	= new Node(updater.get(), getObj(VisualDsp).getTimeProcessor());
    spectProc 	= new Node(updater.get(), getObj(VisualDsp).getFFTProcessor());
    effectsProc = new Node(updater.get(), getObj(VisualDsp).getFXProcessor());

    univNode 	= new Node(updater.get());
    allButFX 	= new Node(updater.get());
    wshpDsp 	= new Node(updater.get());
    irModelDsp 	= new Node(updater.get());

    unisonItr 	= new Node(updater.get());
    unison		= new Node(updater.get());
    ctrlNode	= new Node(updater.get());

    envDlg		= new Node(updater.get(), envelopeDelegate.get());
    spectDlg	= new Node(updater.get(), spectDelegate.get());
    scratchRast	= new Node(updater.get(), scratchUpdate.get());
    morphNode   = new Node(updater.get(), morphUpdate.get());
    synthNode   = new Node(updater.get());

    timeUIs 	= new Node(updater.get());
    spectUIs 	= new Node(updater.get());

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

void CycleUpdater::collectTrackedNodes(Array<Node*>& nodes) const {
    nodes.add(time2Itr);
    nodes.add(time3Itr);
    nodes.add(spect2Itr);
    nodes.add(spect3Itr);
    nodes.add(env2Itr);
    nodes.add(env3Itr);
    nodes.add(irModelItr);
    nodes.add(wshpItr);
    nodes.add(guideCurveItr);
    nodes.add(derivUI);
    nodes.add(time2Rast);
    nodes.add(guideCurveRast);
    nodes.add(irModelRast);
    nodes.add(wshpRast);
    nodes.add(envProc);
    nodes.add(timeProc);
    nodes.add(spectProc);
    nodes.add(effectsProc);
    nodes.add(univNode);
    nodes.add(allButFX);
    nodes.add(wshpDsp);
    nodes.add(irModelDsp);
    nodes.add(unisonItr);
    nodes.add(unison);
    nodes.add(ctrlNode);
    nodes.add(envDlg);
    nodes.add(spectDlg);
    nodes.add(scratchRast);
    nodes.add(morphNode);
    nodes.add(synthNode);
    nodes.add(eqlzerUI);
    nodes.add(timeUIs);
    nodes.add(spectUIs);
}

void CycleUpdater::validateUpdateGraph() const {
    Array<Node*> trackedNodes;
    collectTrackedNodes(trackedNodes);

    std::set<Node*> trackedNodeSet;
    std::set<Node*> reachableNodes;
    std::set<Node*> headNodeSet;
    std::set<Node*> startingNodeSet;
    std::map<Updateable*, Node*> represented;

    for (auto* node : trackedNodes) {
        if (node == nullptr) {
            continue;
        }

        trackedNodeSet.insert(node);

        if (Updateable* target = node->getTarget()) {
            represented[target] = node;
        }
    }

    // These updateables are still executed transitively by delegate nodes rather than
    // owning dedicated graph nodes. Keep them explicit here while validating coverage.
    registerRepresentedTarget(represented, const_cast<EnvVolumeRast*>(&getObj(EnvVolumeRast)), envDlg);
    registerRepresentedTarget(represented, const_cast<EnvPitchRast*>(&getObj(EnvPitchRast)), envDlg);
    registerRepresentedTarget(represented, const_cast<EnvScratchRast*>(&getObj(EnvScratchRast)), scratchRast);
    registerRepresentedTarget(represented, const_cast<EnvWavePitchRast*>(&getObj(EnvWavePitchRast)), envDlg);
    registerRepresentedTarget(represented, const_cast<E3Rasterizer*>(&getObj(E3Rasterizer)), envDlg);
    registerRepresentedTarget(represented, const_cast<SpectRasterizer*>(&getObj(SpectRasterizer)), spectDlg);
    registerRepresentedTarget(represented, const_cast<PhaseRasterizer*>(&getObj(PhaseRasterizer)), spectDlg);
    registerRepresentedTarget(represented, const_cast<Multisample*>(&getObj(Multisample)), morphNode);

    for (auto* headNode : updater->getGraph().getHeadNodes()) {
        if (headNode != nullptr) {
            headNodeSet.insert(headNode);
        }
    }

    for (const auto& entry : updater->getStartingNodes()) {
        Node* startingNode = entry.second;
        if (startingNode == nullptr) {
            continue;
        }

        startingNodeSet.insert(startingNode);
        traverseReachable(startingNode, reachableNodes);

        for (auto* markedNode : startingNode->getMarkedNodes()) {
            traverseReachable(markedNode, reachableNodes);
        }
    }

    StringArray missingNodes;
    StringArray disconnectedNodes;
    StringArray orphanNodes;

    for (auto* accessor : getSingletonRepo()->getObjects()) {
        auto* updateable = dynamic_cast<Updateable*>(accessor);
        if (updateable == nullptr) {
            continue;
        }

        if (accessor->getName() == "PathRepo") {
            continue;
        }

        if (represented.find(updateable) == represented.end()) {
            missingNodes.add(accessor->getName());
        }
    }

    for (auto* node : trackedNodes) {
        if (node == nullptr || node->getTarget() == nullptr) {
            continue;
        }

        bool isHead = headNodeSet.find(node) != headNodeSet.end();
        bool isStartingNode = startingNodeSet.find(node) != startingNodeSet.end();
        bool hasParents = !node->getParents().isEmpty();
        bool isReachable = reachableNodes.find(node) != reachableNodes.end();
        String nodeName = getUpdateableName(node->getTarget());

        if (!isHead && !isStartingNode && !hasParents) {
            orphanNodes.add(nodeName);
        }

        if (!isReachable) {
            disconnectedNodes.add(nodeName);
        }
    }

    if (missingNodes.isEmpty() && disconnectedNodes.isEmpty() && orphanNodes.isEmpty()) {
        info("CycleUpdater graph validation passed\n");
        std::cerr << "CycleUpdater graph validation passed\n";
        return;
    }

    if (!missingNodes.isEmpty()) {
        info("CycleUpdater missing nodes: " << missingNodes.joinIntoString(", ") << "\n");
        std::cerr << "CycleUpdater missing nodes: " << missingNodes.joinIntoString(", ") << '\n';
    }

    if (!orphanNodes.isEmpty()) {
        info("CycleUpdater orphan nodes: " << orphanNodes.joinIntoString(", ") << "\n");
        std::cerr << "CycleUpdater orphan nodes: " << orphanNodes.joinIntoString(", ") << '\n';
    }

    if (!disconnectedNodes.isEmpty()) {
        info("CycleUpdater disconnected nodes: " << disconnectedNodes.joinIntoString(", ") << "\n");
        std::cerr << "CycleUpdater disconnected nodes: " << disconnectedNodes.joinIntoString(", ") << '\n';
    }

    jassertfalse;
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


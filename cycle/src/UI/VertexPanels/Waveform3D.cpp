#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <UI/Widgets/CalloutUtils.h>
#include <UI/Layout/DynamicSizeContainer.h>
#include <Binary/Gradients.h>

#include "Waveform3D.h"

#include <UI/Panels/OpenGLPanel3D.h>

#include "../CycleDefs.h"
#include "../Panels/Morphing/MorphPanel.h"
#include "../Panels/VertexPropertiesPanel.h"
#include "../Widgets/Controls/MeshSelector.h"
#include "../Widgets/Controls/Spacers.h"
#include "../../App/CycleTour.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Inter/WaveformInter2D.h"
#include "../../Inter/WaveformInter3D.h"
#include "../../UI/Effects/IrModellerUI.h"
#include "../../UI/Panels/ModMatrixPanel.h"
#include "../../UI/Panels/PlaybackPanel.h"
#include "../../UI/VisualDsp.h"
#include "../../Util/CycleEnums.h"

Waveform3D::Waveform3D(SingletonRepo* repo) : 
        Panel3D				(repo, "Waveform3D", this, false, true)
    ,	SingletonAccessor	(repo, "Waveform3D")
    ,	LayerSelectionClient(repo)
    ,	Worker				(repo, "Waveform3D")
    ,	model				(5, 1, this, repo, "Auto-model sample waveshape")
    ,	deconvolve			(3, 4, this, repo, "Deconvolve sample waveshape to impulse response")
    ,	spacer8(8) {
}

Waveform3D::~Waveform3D() {
    meshSelector = nullptr;
}

void Waveform3D::init() {
    Image blue 		= PNGImageFormat::loadFrom(Gradients::blue_png, Gradients::blue_pngSize);
    surfInteractor 	= &getObj(WaveformInter3D);
    interactor3D  	= surfInteractor;
    interactor		= interactor3D;

    zoomPanel->tendZoomToCentre = false;
    zoomPanel->panelComponentChanged(openGL.get(), nullptr);

    gradient.read(blue, true, false);
    paramGroup->addSlider(layerPan  = new HSlider(repo, "pan", "Layer pan", true));
    paramGroup->addSlider(layerFine = new HSlider(repo, "fine", "Layer fine tune", true));
    paramGroup->listenToKnobs();

    createNameImage("Waveform Surface", false, true);
    deconvolve.setExpandedSize(26);
    model.setExpandedSize(26);

    panelControls = std::make_unique<PanelControls>(repo, interactor3D, this, this, "Waveshape Layer");
    panelControls->addLayerItems(this, false);
    panelControls->addEnablementIcon();
    panelControls->addLeftItem(&deconvolve, true);
    panelControls->addAndMakeVisible(&deconvolve);
    panelControls->addLeftItem(&spacer8);
    panelControls->addLeftItem(&model, true);
    panelControls->addAndMakeVisible(&model);
    panelControls->addLeftItem(&spacer8);
    panelControls->addScratchSelector();
    panelControls->addSlider(layerPan);
    panelControls->drawLabel = true;

    meshSelector = std::make_unique<MeshSelector<Mesh>>(repo, surfInteractor->getSelectionClient(), "surf", true, true);
    panelControls->addMeshSelector(meshSelector.get());

    layerFine->setRange(0, 1, 0.025);
    layerFine->addListener(this);
    layerPan->setRange(0, 1, 0.025);
    layerPan->addListener(this);
    layerPan->setVelocityModeParameters();
}

void Waveform3D::panelResized() {
    Panel::panelResized();

    getObj(VisualDsp).surfaceResized();
}

void Waveform3D::buttonClicked(Button* button) {
    getObj(EditWatcher).setHaveEditedWithoutUndo(true);

    auto& meshLib = getObj(MeshLibrary);
    MeshLibrary::LayerGroup& timeGroup = meshLib.getGroup(LayerGroups::GroupTime);

    if(button == &panelControls->addRemover.add || button == &panelControls->addRemover.remove) {
        bool forceUpdate = false;

        {
            ScopedLock lock(getObj(SynthAudioSource).getLock());

            if (button == &panelControls->addRemover.add) {
                meshLib.addLayer(interactor->layerType);
                getObj(ModMatrixPanel).layerAdded(interactor->layerType, timeGroup.size() - 1);
            } else if (button == &panelControls->addRemover.remove) {
                int layerIdx = timeGroup.current;
                bool isLast = timeGroup.size() == 1;

                forceUpdate = meshLib.removeLayerKeepingOne(LayerGroups::GroupTime, layerIdx);

                if(! isLast) {
                    getObj(ModMatrixPanel).layerRemoved(interactor->layerType, layerIdx);
                }
            }
        }

        panelControls->refreshSelector(forceUpdate);

        if(forceUpdate) {
            triggerRefreshUpdate();
        }

        getObj(EditWatcher).setHaveEditedWithoutUndo(true);
    } else if (button == &panelControls->enableCurrent) {
        MeshLibrary::Properties* props = getCurrentProperties();

        props->active ^= true;
        panelControls->enableCurrent.setHighlit(props->active);

        if(getSetting(TimeEnabled)) {
            doUpdate(SourceWaveform3D);
        }
    } else if (button == &deconvolve) {
        if(getSetting(WaveLoaded) && getSetting(DrawWave)) {
            getObj(IrModellerUI).deconvolve();
        } else {
            showMsg("You must first be in sample view ('w').");
        }
    } else if (button == &model) {
        if(getSetting(WaveLoaded) && getSetting(DrawWave)) {
            getObj(WaveformInter2D).modelAudioCycle();
        } else {
            showMsg("You must first be in sample view ('w').");
        }
    }

    if(button != &deconvolve) {
        getObj(SynthAudioSource).enablementChanged();
    }
}

void Waveform3D::reset() {
    panelControls->resetSelector();
}

void Waveform3D::layerChanged() {
    progressMark

    interactor->state.reset();
    interactor->clearSelectedAndCurrent();

    getObj(WaveformInter2D).state.reset();
    getObj(VertexPropertiesPanel).updateSliderValues(true);

    panelControls->enableCurrent.setHighlit(getObj(MeshLibrary).getCurrentProps(LayerGroups::GroupTime)->active);

    updateScratchComboBox();
    setKnobValuesImplicit();

    getObj(TimeRasterizer).setMesh(interactor->getMesh());
    getObj(TimeRasterizer).update(Update);
    getObj(WaveformInter2D).update(Update);
    getObj(WaveformInter3D).shallowUpdate();
}

void Waveform3D::setKnobValuesImplicit() {
    MeshLibrary::Properties* props = getObj(MeshLibrary).getCurrentProps(LayerGroups::GroupTime);

    layerPan->setValue(props->pan, dontSendNotification);
    layerFine->setValue(props->fineTune, dontSendNotification);
}

bool Waveform3D::updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) {
    MeshLibrary::Properties* props = getObj(MeshLibrary).getCurrentProps(LayerGroups::GroupTime);

    switch (knobIndex) {
        case Pan:	props->pan 		= knobValue; break;
        case Fine: 	props->fineTune = knobValue; break;
        default: break;
    }

    return knobIndex == Pan;
}

bool Waveform3D::shouldTriggerGlobalUpdate(Slider* slider) {
    bool layerEnabled = panelControls->enableCurrent.isHighlit();

    return (getSetting(ViewStage) >= ViewStages::PostEnvelopes && layerEnabled);
}

void Waveform3D::restoreDetail() {
    interactor->restoreDetail();
}

void Waveform3D::reduceDetail() {
    if(getSetting(DrawWave) == 0) {
        interactor->reduceDetail();
    }
}

void Waveform3D::doGlobalUIUpdate(bool force) {
    interactor->doGlobalUIUpdate(force);
}

//// compatibility code
bool Waveform3D::readXML(const XmlElement* element) {
    // TODO replace with mesh library hookin
    ScopedLock sl2(layerLock);

    XmlElement* timeDomainElem = element->getChildByName("TimeDomainProperties");

    auto& meshLib = getObj(MeshLibrary);
    int layerSize = meshLib.getGroup(LayerGroups::GroupTime).size();

    if (timeDomainElem) {
        bool first = false;

        for(auto timePropsElem : timeDomainElem->getChildWithTagNameIterator("TimeProperties")) {
            MeshLibrary::Properties props;

            props.pan 			= timePropsElem->getDoubleAttribute("pan", 0.5);
            props.fineTune 		= timePropsElem->getDoubleAttribute("fine", 0.5);
            props.scratchChan 	= timePropsElem->getIntAttribute("scratchChannel", 0);
            props.active 		= timePropsElem->getBoolAttribute("isEnabled", true);

            if(first) {
                panelControls->enableCurrent.setHighlit(props.active);
                first = false;
            }
//			props.pan.setValueDirect(		timePropsElem->getDoubleAttribute("pan", 	0.5));
//			props.fineTune.setValueDirect(	timePropsElem->getDoubleAttribute("fine", 	0.5));
        }
    }

    panelControls->resetSelector();
    setKnobValuesImplicit();

    return true;
}

void Waveform3D::writeXML(XmlElement* element) const {
}

int Waveform3D::getLayerType() {
    return interactor->layerType;
}

int Waveform3D::getWindowWidthPixels() {
    return int(sxnz(1.f) - sxnz(0.f));
}

MeshLibrary::Properties* Waveform3D::getCurrentProperties() {
    return getObj(MeshLibrary).getCurrentProps(LayerGroups::GroupTime);
}

void Waveform3D::scratchChannelSelected(int channel) {
    getCurrentProperties()->scratchChan = channel;
}

void Waveform3D::updateScratchComboBox() {
    int scratchChannel = getCurrentProperties()->scratchChan;
    panelControls->setScratchSelector(scratchChannel);
}

bool Waveform3D::validateScratchChannels() {
    panelControls->populateScratchSelector();

    int numChannels = getObj(MeshLibrary).getGroup(LayerGroups::GroupScratch).size();
    bool changed = false;

    MeshLibrary::LayerGroup& timeGroup = getObj(MeshLibrary).getGroup(LayerGroups::GroupTime);

    for (int i = 0; i < timeGroup.size(); ++i) {
        MeshLibrary::Layer& layer = timeGroup[i];

        if (layer.props->scratchChan >= numChannels) {
            layer.props->scratchChan = CommonEnums::Null;
            changed = true;
        }
    }

    updateScratchComboBox();

    return changed;
}

void Waveform3D::populateLayerBox() {
//	panelControls->populateLayerBox();
}

int Waveform3D::getLayerScratchChannel() {
    return getCurrentProperties()->scratchChan;
}

int Waveform3D::getNumActiveLayers() {
    MeshLibrary::LayerGroup& timeGroup = getObj(MeshLibrary).getGroup(LayerGroups::GroupTime);

    int numActiveLayers = 0;
    for (int i = 0; i < timeGroup.size(); ++i) {
        if(timeGroup[i].props->active && timeGroup[i].mesh->hasEnoughCubesForCrossSection()) {
            ++numActiveLayers;
        }
    }

    return numActiveLayers;
}

bool Waveform3D::shouldDrawGrid() {
    if (getSetting(ViewStage) >= ViewStages::PostSpectrum) {
        return true;
    }

    return getNumActiveLayers() > 0;
}

void Waveform3D::updateSmoothParametersToTarget(int voiceIndex) {
    MeshLibrary::LayerGroup& timeGroup = getObj(MeshLibrary).getGroup(LayerGroups::GroupTime);

    for (int i = 0; i < timeGroup.size(); ++i) {
        MeshLibrary::Layer& layer = timeGroup[i];

//		layer.props->pos[voiceIndex].red.updateToTarget();
//		layer.props->pos[voiceIndex].blue.updateToTarget();
    }
}

void Waveform3D::updateLayerSmoothedParameters(int voiceIndex, int deltaSamples) {
    MeshLibrary::LayerGroup& timeGroup = getObj(MeshLibrary).getGroup(LayerGroups::GroupTime);

    for (int i = 0; i < timeGroup.size(); ++i) {
        MeshLibrary::Layer& layer = timeGroup[i];
//		layer.props->pos[voiceIndex].red.update(deltaSamples);
//		layer.props->pos[voiceIndex].blue.update(deltaSamples);
    }
}

void Waveform3D::updateSmoothedParameters(int deltaSamples) {
    MeshLibrary::LayerGroup& timeGroup = getObj(MeshLibrary).getGroup(LayerGroups::GroupTime);

    for (int i = 0; i < timeGroup.size(); ++i) {
        MeshLibrary::Layer& layer = timeGroup[i];

//		layer.props->pan.update(deltaSamples);
//		layer.props->fineTune.update(deltaSamples);
    }
}

void Waveform3D::setLayerParameterSmoothing(int voiceIndex, bool smooth) {
    MeshLibrary::LayerGroup& timeGroup = getObj(MeshLibrary).getGroup(LayerGroups::GroupTime);

    for (int i = 0; i < timeGroup.size(); ++i) {
        MeshLibrary::Layer& layer = timeGroup[i];

//		layer.props->pos[voiceIndex].red.setSmoothingActivity(smooth);
//		layer.props->pos[voiceIndex].blue.setSmoothingActivity(smooth);
    }
}

Component* Waveform3D::getComponent(int which) {
    switch (which) {
        case CycleTour::TargLayerEnable: 	return &panelControls->enableCurrent;
        case CycleTour::TargLayerAdder: 	return &panelControls->addRemover;
        case CycleTour::TargScratchBox: 	return &panelControls->scratchSelector;
        case CycleTour::TargLayerSlct: 		return panelControls->layerSelector.get();
        case CycleTour::TargPan:			return layerPan;
        case CycleTour::TargDeconv:			return &deconvolve;
        case CycleTour::TargModelCycle:		return &model;
        case CycleTour::TargPhaseUp:		return nullptr;
        case CycleTour::TargMeshSelector:	return meshSelector.get();
        default: break;
    }

    return nullptr;
}

void Waveform3D::triggerButton(int button) {
    switch (button) {
        case CycleTour::IdBttnEnable: 	buttonClicked(&panelControls->enableCurrent); 		break;
        case CycleTour::IdBttnAdd: 	 	buttonClicked(&panelControls->addRemover.add); 		break;
        case CycleTour::IdBttnRemove: 	buttonClicked(&panelControls->addRemover.remove); 	break;
        case CycleTour::IdBttnDeconv: 	buttonClicked(&deconvolve); 						break;
        case CycleTour::IdBttnModel: 	buttonClicked(&model); 								break;
        default: break;
    }
}

void Waveform3D::sliderValueChanged(Slider* slider) {
    MeshLibrary::Properties* props = getCurrentProperties();

    if (slider == layerFine) {
        props->fineTune = slider->getValue();
    } else if (slider == layerPan) {
        props->pan = slider->getValue();
    }
}

void Waveform3D::doZoomExtra(bool commandDown) {
}

Buffer<float> Waveform3D::getColumnArray() {
    return getObj(VisualDsp).getTimeArray();
}

const vector <Column>& Waveform3D::getColumns() {
    return getObj(VisualDsp).getTimeColumns();
}

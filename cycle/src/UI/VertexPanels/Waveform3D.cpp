#include <App/EditWatcher.h>
#include <App/Doc/PresetJson.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Binary/Gradients.h>
#include <Curve/Mesh/Mesh.h>
#include <UI/Widgets/CalloutUtils.h>
#include <UI/Layout/DynamicSizeContainer.h>
#include <UI/Panels/OpenGLPanel3D.h>

#include "Waveform3D.h"
#include "Spectrum3D.h"

#include "../CycleDefs.h"
#include "../Panels/Morphing/MorphPanel.h"
#include "../Panels/VertexPropertiesPanel.h"
#include "../Widgets/Controls/MeshSelector.h"
#include "../Widgets/Controls/Spacers.h"
#include "../../App/CycleTour.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Curve/Rasterization/Rasterizer/GraphicRasterizer.h"
#include "../../Inter/WaveformInter2D.h"
#include "../../Inter/WaveformInter3D.h"
#include "../../UI/Effects/IrModellerUI.h"
#include "../../UI/Panels/ModMatrixPanel.h"
#include "../../UI/Panels/PlaybackPanel.h"
#include "../../UI/VisualDsp.h"
#include "../../Curve/Rasterization/Rasterizer/TimeColumnRasterizer.h"
#include "../../Util/CycleEnums.h"

#define panelName "Waveform3D"

namespace {
    String viewStageName(int stage) {
        switch (stage) {
            case ViewStages::PreProcessing:    return "preProcessing";
            case ViewStages::PostEnvelopes:    return "postEnvelopes";
            case ViewStages::PostSpectrum:     return "postSpectrum";
            case ViewStages::PostFX:           return "postFX";
            default:                           break;
        }

        return "unknown";
    }

    var layerPropsState(MeshLibrary::Properties* props) {
        auto json = PresetJson::object();

        if (props == nullptr) {
            json->setProperty("resolved", false);
            return PresetJson::toVar(json);
        }

        json->setProperty("resolved", true);
        json->setProperty("active", props->active);
        json->setProperty("mode", props->mode);
        json->setProperty("scratchChannel", props->scratchChan);
        json->setProperty("pan", props->pan.getTargetValue());
        json->setProperty("fineTune", props->fineTune.getTargetValue());
        json->setProperty("gain", props->gain);
        json->setProperty("range", props->range);
        return PresetJson::toVar(json);
    }

    var meshState(Mesh* mesh) {
        auto json = PresetJson::object();
        json->setProperty("resolved", mesh != nullptr);

        if (mesh != nullptr) {
            json->setProperty("vertices", mesh->getNumVerts());
            json->setProperty("cubes", mesh->getNumCubes());
            json->setProperty("hasEnoughCubesForCrossSection", mesh->hasEnoughCubesForCrossSection());
        }

        return PresetJson::toVar(json);
    }
}

Waveform3D::Waveform3D(SingletonRepo* repo) :
        Panel3D				(repo, panelName, this, UpdateSources::SourceWaveform3D, false, true)
    ,	SingletonAccessor	(repo, panelName)
    ,	LayerSelectionClient(repo)
    ,	Worker				(repo, panelName)
    ,	model				(5, 1, this, repo, "Auto-model sample waveshape")
    ,	deconvolve			(3, 4, this, repo, "Deconvolve sample waveshape to impulse response")
    ,	spacer8(8) {
}

Waveform3D::~Waveform3D() {
    meshSelector = nullptr;
}

void Waveform3D::init() {
    Panel3D::init();

    Image blue 		= PNGImageFormat::loadFrom(Gradients::blue_png, Gradients::blue_pngSize);
    surfInteractor 	= &getObj(WaveformInter3D);
    interactor3D  	= surfInteractor;
    setInteractor(interactor3D);
    auto* rasterizer = &getObj(TimeRasterizer);
    interactor3D->setRasterizer(rasterizer);
    surfInteractor->updateRastDims();
    surfInteractor->updateSelectionClient();

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

    meshSelector = std::make_unique<MeshSelector<Mesh>>(repo, surfInteractor->getSelectionClient(), "waveform", "mesh", true, true);
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

void Waveform3D::zoomUpdated(int updateSource) {
    Panel3D::zoomUpdated(updateSource);

    if (updateSource != UpdateSources::SourceWaveform3D &&
        updateSource != UpdateSources::SourceSpectrum3D) {
        return;
    }

    // TODO: this seems to be better served with some kind of publisher/listener pattern
    Spectrum3D& spectrum = getObj(Spectrum3D);
    ZoomRect& source = updateSource == UpdateSources::SourceWaveform3D
                           ? getZoomPanel()->rect
                           : spectrum.getZoomPanel()->rect;
    ZoomRect& dest = updateSource == UpdateSources::SourceWaveform3D
                         ? spectrum.getZoomPanel()->rect
                         : getZoomPanel()->rect;

    dest.x = source.x;
    dest.w = source.w;

    spectrum.getZoomPanel()->panelZoomChanged(false);
    spectrum.updateBackground(false);
    spectrum.bakeTexturesNextRepaint();
    spectrum.repaint();
}

void Waveform3D::buttonClicked(Button* button) {
    getObj(EditWatcher).setHaveEditedWithoutUndo(true);

    auto& meshLib = getObj(MeshLibrary);
    MeshLibrary::LayerGroup& timeGroup = meshLib.getLayerGroup(LayerGroups::GroupTime);

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
            showConsoleMsg("You must first be in sample view ('w').");
        }
    } else if (button == &model) {
        if(getSetting(WaveLoaded) && getSetting(DrawWave)) {
            getObj(WaveformInter2D).modelAudioCycle();
        } else {
            showConsoleMsg("You must first be in sample view ('w').");
        }
    }

    if(button != &deconvolve) {
        getObj(SynthAudioSource).enablementChanged();
    }
}

void Waveform3D::reset() {
    panelControls->resetSelector();
}

void Waveform3D::reconcileLoadedState() {
    if (panelControls == nullptr) {
        return;
    }

    panelControls->refreshSelector();
    panelControls->enableCurrent.setHighlit(getCurrentProperties()->active);
    updateScratchComboBox();
    setKnobValuesImplicit();
}

void Waveform3D::layerChanged() {
    progressMark

    interactor->state.reset();
    interactor->clearSelectedAndCurrent();

    getObj(WaveformInter2D).state.reset();
    getObj(VertexPropertiesPanel).updateSliderValues(true);

    reconcileLoadedState();

    // TODO: setMesh() ?? I thought MeshLibrary served all of this and notified.
    TimeRasterizer& timeRasterizer = getObj(TimeRasterizer);
    timeRasterizer.setMesh(interactor->getMesh());
    timeRasterizer.setNoiseSeed(Cycle::Rasterization::TimeColumnRasterizer::noiseSeedForLayer(
            getObj(MeshLibrary).getCurrentIndex(LayerGroups::GroupTime)));
    // TODO: two updates is slightly weird
    timeRasterizer.update(Update);
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
        case Pan:	props->pan.setTargetValue(knobValue); break;
        case Fine: 	props->fineTune.setTargetValue(knobValue); break;
        default: break;
    }

    // did anything significant
    return knobIndex == Pan;
}

bool Waveform3D::shouldTriggerGlobalUpdate(Slider* slider) {
    bool layerEnabled = panelControls->enableCurrent.isHighlit();

    return (getSetting(ViewStage) >= ViewStages::PostEnvelopes && layerEnabled);
}

bool Waveform3D::isScratchApplicable() {
    return getSetting(ViewStage) >= ViewStages::PostEnvelopes;
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

void Waveform3D::layerChanged(int layerGroup, int index) {
    if (layerGroup != LayerGroups::GroupTime) {
        return;
    }

    reconcileLoadedState();
}

void Waveform3D::layerGroupAdded(int layerGroup) {
    layerChanged(layerGroup, -1);
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

    int numChannels = getObj(MeshLibrary).getLayerGroup(LayerGroups::GroupScratch).size();
    bool changed = false;

    MeshLibrary::LayerGroup& timeGroup = getObj(MeshLibrary).getLayerGroup(LayerGroups::GroupTime);

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
    MeshLibrary::LayerGroup& timeGroup = getObj(MeshLibrary).getLayerGroup(LayerGroups::GroupTime);

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

juce::Component* Waveform3D::getComponent(int which) {
    switch (which) {
        case CycleTour::TargLayerEnable: 	return &panelControls->enableCurrent;
        case CycleTour::TargLayerAdder: 	return &panelControls->addRemover;
        case CycleTour::TargLayerAddButton: return &panelControls->addRemover.add;
        case CycleTour::TargLayerRemoveButton: return &panelControls->addRemover.remove;
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

CriticalSection& Waveform3D::getGridLock() {
    int stage = getSetting(ViewStage);

    if (getSetting(DrawWave)) {
        return getObj(VisualDsp).getColumnLock(VisualDsp::EnvColType);
    }

    if (stage == ViewStages::PostFX) {
        return getObj(VisualDsp).getColumnLock(VisualDsp::FXColType);
    }
    if (stage == ViewStages::PreProcessing) {
        return getObj(VisualDsp).getColumnLock(VisualDsp::TimeColType);
    }

    return getObj(VisualDsp).getColumnLock(VisualDsp::EnvColType);
}

bool Waveform3D::isSurfaceDetailReduced() {
    return getObj(TimeRasterizer).isDetailReduced();
}

var Waveform3D::exportAutomationState() const {
    auto json = PresetJson::object();
    auto view = PresetJson::object();
    auto zoom = PresetJson::object();
    auto layer = PresetJson::object();

    auto& meshLib = const_cast<MeshLibrary&>(getObj(MeshLibrary));
    auto& group = meshLib.getLayerGroup(LayerGroups::GroupTime);
    int currentLayer = meshLib.getCurrentIndex(LayerGroups::GroupTime);
    int viewStage = getSettingValue(ViewStage);

    json->setProperty("schema", "Waveform3D.v1");
    json->setProperty("layerGroup", LayerGroups::GroupTime);
    json->setProperty("layerGroupName", "time");

    view->setProperty("viewStage", viewStage);
    view->setProperty("viewStageName", viewStageName(viewStage));
    view->setProperty("drawWave", bool(getSettingValue(DrawWave)));
    view->setProperty("waveLoaded", bool(getSettingValue(WaveLoaded)));
    view->setProperty("shouldDrawGrid", const_cast<Waveform3D*>(this)->shouldDrawGrid());
    view->setProperty("windowWidthPixels", const_cast<Waveform3D*>(this)->getWindowWidthPixels());
    json->setProperty("view", PresetJson::toVar(view));

    ZoomRect& zoomRect = const_cast<Waveform3D*>(this)->getZoomPanel()->getZoomRect();
    zoom->setProperty("x", zoomRect.x);
    zoom->setProperty("y", zoomRect.y);
    zoom->setProperty("width", zoomRect.w);
    zoom->setProperty("height", zoomRect.h);
    json->setProperty("zoom", PresetJson::toVar(zoom));

    auto spectrumZoom = PresetJson::object();
    ZoomRect& spectrumZoomRect = const_cast<Spectrum3D&>(getObj(Spectrum3D)).getZoomPanel()->getZoomRect();
    spectrumZoom->setProperty("x", spectrumZoomRect.x);
    spectrumZoom->setProperty("y", spectrumZoomRect.y);
    spectrumZoom->setProperty("width", spectrumZoomRect.w);
    spectrumZoom->setProperty("height", spectrumZoomRect.h);
    json->setProperty("spectrumZoom", PresetJson::toVar(spectrumZoom));

    layer->setProperty("current", currentLayer);
    layer->setProperty("count", group.size());
    layer->setProperty("activeCount", const_cast<Waveform3D*>(this)->getNumActiveLayers());
    layer->setProperty("selectedCount", int(group.selected.size()));
    layer->setProperty("scratchChannel", const_cast<Waveform3D*>(this)->getLayerScratchChannel());
    layer->setProperty("rasterizerNoiseSeed", getObj(TimeRasterizer).getNoiseSeed());
    layer->setProperty("currentProperties", layerPropsState(meshLib.getCurrentProps(LayerGroups::GroupTime)));
    layer->setProperty("currentMesh", meshState(meshLib.getCurrentMesh(LayerGroups::GroupTime)));
    json->setProperty("layer", PresetJson::toVar(layer));

    return PresetJson::toVar(json);
}

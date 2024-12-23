#include <cmath>
#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Binary/Gradients.h>
#include <Curve/Mesh.h>
#include <UI/Widgets/CalloutUtils.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/Knob.h>
#include <Util/LogRegions.h>
#include <Util/StringFunction.h>

#include "Spectrum2D.h"
#include "Spectrum3D.h"

#include <UI/Panels/OpenGLPanel3D.h>

#include "../CycleDefs.h"
#include "../Panels/Morphing/MorphPanel.h"
#include "../Panels/VertexPropertiesPanel.h"
#include "../VertexPanels/Waveform3D.h"
#include "../Widgets/Controls/LayerAddRemover.h"
#include "../Widgets/Controls/MeshSelector.h"
#include "../Widgets/Controls/Spacers.h"
#include "../../App/CycleTour.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Curve/GraphicRasterizer.h"
#include "../../Inter/SpectrumInter2D.h"
#include "../../Inter/SpectrumInter3D.h"
#include "../../UI/Panels/ModMatrixPanel.h"
#include "../../UI/VisualDsp.h"
#include "../../Util/CycleEnums.h"


Spectrum3D::Spectrum3D(SingletonRepo* repo) : 
        Panel3D				(repo, "Spectrum3D", this, true, false)
    ,	SingletonAccessor	(repo, "Spectrum3D")
    ,	LayerSelectionClient(repo)
    ,	Worker              (repo, "Spectrum3D")
    ,	phaseIcon			(3, 6, this, repo, "Phase spectrum", "h")
    ,	magsIcon			(1, 4, this, repo, "Magnitude spectrum", "h")
    ,	additiveIcon		(4, 8, this, repo, "Set Layer Mode to Spectral-Additive")
    ,	subtractiveIcon		(5, 8, this, repo, "Set Layer Mode to Spectral-Filtering")
    ,	spacer6(6) {
}

void Spectrum3D::init() {
    Image alum 		= PNGImageFormat::loadFrom(Gradients::burntalum_png, Gradients::burntalum_pngSize);

    interactor3D 	= &getObj(SpectrumInter3D);
    meshLib 		= &getObj(MeshLibrary);
    spect2D 		= &getObj(Spectrum2D);
    interactor 		= interactor3D;

    float margin 	= getRealConstant(SpectralMargin);

    volumeScale		= 1.25f;
    volumeTrans		= 0.f;
    haveLogarithmicY= true;

    gradient.read(alum, false, true);
    phaseGradient.read(alum, false, true);

    paramGroup->addSlider(layerPan = new HSlider(repo, "pan", "Layer pan", true));
    paramGroup->addSlider(dynRangeKnob = new HSlider(repo, "range", "Layer dynamic range", true));

    layerPan	->setValue(0.5, dontSendNotification);
    dynRangeKnob->setValue(0.5, dontSendNotification);
    paramGroup	->listenToKnobs();

    createNameImage("Magn. Spectrogram");
    createNameImage("Phase Spectrogram", true);

    zoomPanel->rect.h = 0.95;
    zoomPanel->rect.yMinimum = -margin;
    zoomPanel->rect.yMaximum = 1 + margin;
    zoomPanel->panelComponentChanged(openGL.get(), nullptr);

    panelControls = std::make_unique<PanelControls>(repo, interactor3D, this, this, "Magnitude Spectrum Layer");
    panelControls->addSlider(layerPan);
    panelControls->addSlider(dynRangeKnob);
    panelControls->addLayerItems(this, true);
    panelControls->addEnablementIcon();
    panelControls->enableCurrent.setHighlit(true);
    panelControls->drawLabel = true;

    Component* tempMode[] = { &magsIcon, 	 &phaseIcon };
    Component* tempOper[] = { &additiveIcon, &subtractiveIcon };

    CalloutUtils::addRetractableCallout(operCO, operPO, repo, 4, 8, tempOper,
                                        numElementsInArray(tempOper), panelControls.get(), false);

    operCO->translateUp1 = true;

    CalloutUtils::addRetractableCallout(modeCO, modePO, repo, 3, 7, tempMode,
                                        numElementsInArray(tempMode), panelControls.get(), true);

    panelControls->addDomainItems(modeCO.get());
    panelControls->addLeftItem(operCO.get(), true);
    panelControls->addLeftItem(&spacer6);
    panelControls->addScratchSelector();

    meshSelector = std::make_unique<MeshSelector<Mesh>>(repo, getObj(SpectrumInter3D).getSelectionClient(), "f3d", true, true);
    panelControls->addMeshSelector(meshSelector.get());

    additiveIcon.setHighlit(true);
    layerPan->setRange(0, 1, 0.025);
    layerPan->addListener(this);

    dynRangeKnob->setRange(0, 1, 0.025);
    dynRangeKnob->addListener(this);

    using namespace Ops;
    dynStr = StringFunction().mul(12.f).sub(-4.f).powRev(2.f).abs().log().mul(6.f / logf(2.f));
    dynRangeKnob->setStringFunctions(dynStr, dynStr.withPostString(" dB"));

    magsIcon.setHighlit(true);
}

void Spectrum3D::drawPlaybackLine() {
}

void Spectrum3D::updateBackground(bool onlyVerticalBackground) {
    Panel::updateBackground(true);

    int midiKey = getObj(MorphPanel).getCurrentMidiKey();
    Buffer<float> ramp = getObj(LogRegions).getRegion(midiKey);

    int factor = 8;
    int minorSize = (ramp.size() - factor) / factor;

    if (minorSize > 0) {
        jassert(minorSize <= maxMinorSize);
        horzMinorLines.resize(minorSize);
        horzMinorLines.downsampleFrom(ramp.offset(factor), factor);
    }
}

void Spectrum3D::panelResized() {
    Panel::panelResized();
}

vector <Color>& Spectrum3D::getGradientColours() {
    return getSetting(MagnitudeDrawMode) ? gradient.getColours() : phaseGradient.getColours();
}

void Spectrum3D::reset() {
    panelControls->resetSelector();
}

void Spectrum3D::modeChanged(bool isMags, bool updateInteractors) {
    interactor->layerType = isMags ? LayerGroups::GroupSpect : LayerGroups::GroupPhase;

    auto& f2 = getObj(SpectrumInter2D);
    auto& f3 = getObj(SpectrumInter3D);

    f2.layerType = interactor->layerType;
    f3.layerType = interactor->layerType;

    f2.vertexProps.dimensionNames.set(Vertex::Amp, isMags ? "Magn" : "Phase");
    f3.vertexProps.dimensionNames = f2.vertexProps.dimensionNames;

    MeshRasterizer* rast = isMags ?
            (MeshRasterizer*) &getObj(SpectRasterizer) :
            (MeshRasterizer*) &getObj(PhaseRasterizer);

    StringFunction shortStr = isMags ? dynStr : StringFunction().mul(5.f).pow(IPP_E);
    StringFunction longStr = isMags ? shortStr.withPostString(" dB") : shortStr.withPostString(L" \u03c0");

    dynRangeKnob->setStringFunctions(shortStr, longStr);

    getSetting(MagnitudeDrawMode) = isMags;
    updateColours();

    panelControls->enableCurrent.setHighlit(getCurrentProperties()->active);
    panelControls->addRemover.setLayerString(isMags ? "Magnitude Spectrum Layer" :
                                                      "Phase Spectrum Layer");

    int nameTex = isMags ? NameTexture : NameTextureB;
    setNameTextureId(nameTex);
    spect2D->setNameTextureId(nameTex);
    updateScratchComboBox();

    dynRangeKnob->setName(isMags ? "range" : "width");

    f3.setRasterizer(rast);
    f2.setRasterizer(rast);
    f3.updateSelectionClient();
    panelControls->refreshSelector();

    if (updateInteractors) {
        f3.update(Update);
        f2.updateDspSync();
        f2.update(Update);
    }
}

void Spectrum3D::buttonClicked(Button* button) {
    Mesh* mesh 			= interactor->getMesh();
    bool drawMagsFlag 	= getSetting(MagnitudeDrawMode) == 1;
    int groupIdx		= drawMagsFlag ? LayerGroups::GroupSpect : LayerGroups::GroupPhase;

    MeshLibrary::Properties* props = getCurrentProperties();
    MeshLibrary::LayerGroup& group = meshLib->getGroup(groupIdx);

    if (button == &magsIcon || button == &phaseIcon) {
        bool isMags = button == &magsIcon;

        if (isMags == (getSetting(MagnitudeDrawMode) == 1)) {
            return;
        }

        modeChanged(isMags, true);

//		getObj(SpectrumInter3D).postUpdateMessage();
    } else if (button == &additiveIcon ||
               button == &subtractiveIcon) {

        bool isAdditive = button == &additiveIcon;
        if(props->mode == Additive && isAdditive)
            return;

        props->mode = isAdditive ? Additive : Subtractive;

        updateColours();

        doUpdate(SourceSpectrum3D);
    } else if (button == &panelControls->addRemover.add ||
               button == &panelControls->addRemover.remove) {
        bool forceUpdate = false;

        {
            ScopedLock lock(getObj(SynthAudioSource).getLock());

            if (button == &panelControls->addRemover.add) {
                meshLib->addLayer(interactor->layerType);
                getObj(ModMatrixPanel).layerAdded(interactor->layerType, -1);
            } else if (button == &panelControls->addRemover.remove) {
                int index = group.current;

                // takes care of decrementing current layer
                forceUpdate = meshLib->removeLayerKeepingOne(interactor->layerType, index);

                if(group.size() > 1) {
                    getObj(ModMatrixPanel).layerRemoved(interactor->layerType, index);
                }
            }
        }

        panelControls->refreshSelector(forceUpdate);

        if(forceUpdate) {
            triggerRefreshUpdate();
        }

        getObj(EditWatcher).setHaveEditedWithoutUndo(true);
    } else if (button == &panelControls->upDownMover.up ||
               button == &panelControls->upDownMover.down) {
        bool isUp = button == &panelControls->upDownMover.up;
        panelControls->moveLayer(isUp);

        doUpdate(SourceSpectrum3D);
    } else if (button == &panelControls->enableCurrent) {
        props->active ^= true;
        panelControls->enableCurrent.setHighlit(props->active);

        enablementsChanged();

        if (getSetting(MagnitudeDrawMode) == 1 && getSetting(FilterEnabled) ||
            getSetting(MagnitudeDrawMode) == 0 && getSetting(PhaseEnabled)) {
            doUpdate(SourceSpectrum3D);
        }
    }

    if(	button == &panelControls->enableCurrent ||
        button == &additiveIcon 				||
        button == &subtractiveIcon) {
        getObj(SynthAudioSource).enablementChanged();
    }

    if(	button != &panelControls->upDownMover.up 	&&
        button != &panelControls->upDownMover.down 	&&
        button != &panelControls->enableCurrent) {
        setIconHighlightImplicit();
    }

    if(button == &magsIcon || button == &phaseIcon) {
        updateKnobValue();
    }
}

void Spectrum3D::layerChanged() {
    progressMark

    interactor->clearSelectedAndCurrent();
    interactor->state.reset();
    getObj(SpectrumInter2D).state.reset();

    updateScratchComboBox();
    setIconHighlightImplicit();
    updateKnobValue();
    updateColours();

    getObj(VertexPropertiesPanel).updateSliderValues(true);

    interactor->getRasterizer()->setMesh(getObj(SpectrumInter3D).getMesh());
    interactor->getRasterizer()->update(UpdateType::Update);

    getObj(SpectrumInter2D).update(UpdateType::Update);
    getObj(SpectrumInter3D).shallowUpdate();
}

int Spectrum3D::getLayerType() {
    return interactor->layerType;
}

void Spectrum3D::setDynRange(int layerIdx, bool isFreq, float value) {
    getPropertiesForLayer(layerIdx, isFreq)->range = value;
}

void Spectrum3D::setPan(int layerIdx, bool isFreq, float value) {
    getPropertiesForLayer(layerIdx, isFreq)->pan = value;
}

void Spectrum3D::setIconHighlightImplicit() {
    bool isMags = getSetting(MagnitudeDrawMode) == 1;

    magsIcon.setHighlit(isMags);
    phaseIcon.setHighlit(! isMags);

    additiveIcon.setApplicable(isMags);
    subtractiveIcon.setApplicable(isMags);

    MeshLibrary::Properties* props = getCurrentProperties();
    panelControls->enableCurrent.setHighlit(props->active);

    if (isMags) {
        additiveIcon.setHighlit(props->mode == Additive);
        subtractiveIcon.setHighlit(props->mode == Subtractive);
    }
}

void Spectrum3D::updateKnobValue() {
    MeshLibrary::Properties* props = getCurrentProperties();

    layerPan->setValue(props->pan, dontSendNotification);
    dynRangeKnob->setValue(props->range, dontSendNotification);
}

void Spectrum3D::updateColours() {
    MeshLibrary::Properties* props = getCurrentProperties();

    bool isMags = getSetting(MagnitudeDrawMode) == 1;
    bool polar = ! isMags || (isMags && props->mode == Subtractive);

    Color yellow(0.85f, 0.68f, 0.23f);
    Color blue(0.44f, 0.605f, 0.88f);

    spect2D->setCurveBipolar(polar);
    spect2D->setColors(yellow, polar ? blue : yellow);
}

void Spectrum3D::writeXML(XmlElement* element) const {
    // TODO
}

bool Spectrum3D::readXML(const XmlElement* element) {
    ScopedLock sl2(layerLock);

    XmlElement* freqDomainElem = element->getChildByName("FreqDomainProperties");

    int magnSize = meshLib->getGroup(LayerGroups::GroupSpect).size();
    int phaseSize = meshLib->getGroup(LayerGroups::GroupPhase).size();

    if (freqDomainElem) {
        XmlElement* magsElem = freqDomainElem->getChildByName("MagsPropertiesSet");
        XmlElement* phaseElem = freqDomainElem->getChildByName("PhasePropertiesSet");

        if (magsElem) {
            for(auto magPropsElem : magsElem->getChildWithTagNameIterator("MagnitudeProperties")) {
                MeshLibrary::Properties props;

                bool isAdditive 	= magPropsElem->getBoolAttribute("additive", true);

                props.mode 			= isAdditive ? Additive : Subtractive;
                props.pan 			= magPropsElem->getDoubleAttribute("pan", 0.5);
                props.range  		= magPropsElem->getDoubleAttribute("dynamicRange", 0.5);
                props.active 		= magPropsElem->getBoolAttribute("isEnabled", true);
                props.scratchChan 	= magPropsElem->getIntAttribute("scratchChannel", 0);

                // meshLib->getGroup(LayerGroups::GroupSpect).layers.push_back(MeshLibrary::Layer);
                // TODO set props to mesh library

            }
        }

        if (phaseElem) {
            for(auto phasePropsElem : phaseElem->getChildWithTagNameIterator("PhaseProperties")) {
                MeshLibrary::Properties props;

                props.pan 			= phasePropsElem->getDoubleAttribute("pan", 0.5);
                props.range 		= phasePropsElem->getDoubleAttribute("dynamicRange", 0.5);
                props.active 		= phasePropsElem->getBoolAttribute("isEnabled", true);
                props.scratchChan 	= phasePropsElem->getIntAttribute("scratchChannel", CommonEnums::Null);

                // TODO set props to mesh library
            }
        }
    }

    // XXX

    panelControls->resetSelector(); //resetLayerBox();

    modeChanged(getSetting(MagnitudeDrawMode) == 1, false);
    setIconHighlightImplicit();
    updateKnobValue();
    enablementsChanged();

    return true;
}

bool Spectrum3D::updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) {
    MeshLibrary::Properties* props = getCurrentProperties();

    switch (knobIndex) {
        case Pan:		props->pan 	 = knobValue; break;
        case DynRange: 	props->range = knobValue; break;
        default: break;
    }

    return true;
}

bool Spectrum3D::shouldTriggerGlobalUpdate(Slider* slider) {
    bool layerEnabled = panelControls->enableCurrent.isHighlit();
    return (getSetting(ViewStage) >= ViewStages::PostSpectrum) && layerEnabled;
}

void Spectrum3D::restoreDetail() {
    interactor->restoreDetail();
}

void Spectrum3D::reduceDetail() {
    if (getSetting(DrawWave) == 0) {
        interactor->reduceDetail();
    }
}

void Spectrum3D::doGlobalUIUpdate(bool force) {
    interactor->doGlobalUIUpdate(force);
}

MeshLibrary::Properties* Spectrum3D::getPropertiesForLayer(int layerIdx, bool isFreq) {
    ScopedLock sl(layerLock);

    int groupIdx = isFreq ? LayerGroups::GroupSpect : LayerGroups::GroupPhase;
    return meshLib->getProps(groupIdx, layerIdx);
}

MeshLibrary::LayerGroup& Spectrum3D::getCurrentGroup() {
    int groupIdx = getSetting(MagnitudeDrawMode) == 1 ? LayerGroups::GroupSpect : LayerGroups::GroupPhase;

    return meshLib->getGroup(groupIdx);
}

MeshLibrary::Properties* Spectrum3D::getCurrentProperties() {
    int groupIdx = getSetting(MagnitudeDrawMode) == 1 ? LayerGroups::GroupSpect : LayerGroups::GroupPhase;

    return meshLib->getCurrentProps(groupIdx);
}

void Spectrum3D::updateSmoothedParameters(int deltaSamples) {
    ScopedLock sl(layerLock);

    // getObj(MeshLibrary).getGroup(Spectrogram).layers;

	// for(int i = 0; i < (int) magnProperties.size(); ++i)
	// {
	// 	LayerProps& props = magnProperties.getReference(i);
	// 	props.pan.update(deltaSamples);
	// 	props.dynamicRange.update(deltaSamples);
	// }
	//
	// for(int i = 0; i < (int) phaseProperties.size(); ++i)
	// {
	// 	LayerProps& props = phaseProperties.getReference(i);
	// 	props.pan.update(deltaSamples);
	// 	props.dynamicRange.update(deltaSamples);
	// }
}

void Spectrum3D::changedToOrFromTimeDimension() {
//	phaseIcon.setApplicable(getSetting(CurrentMorphAxis) == Vertex::Time);
}

void Spectrum3D::populateLayerBox() {
//	panelControls->selectorPanel->populateLayerBox();
}

/*
void Spectrum3D::moveLayerProperties(int fromIndex, int toIndex)
{
    bool isMags = getSetting(MagnitudeDrawMode) == 1;
    ScopedLock sl(layerLock);

    int groupId = isMags ? LayerGroups::GroupSpect : LayerGroups::GroupPhase;
    meshLib->moveLayer(groupId, fromIndex, toIndex);
}
*/

void Spectrum3D::scratchChannelSelected(int channel) {
    MeshLibrary::Properties* props = getCurrentProperties();

    if (props != nullptr)
        props->scratchChan = channel;
}

void Spectrum3D::updateScratchComboBox() {
    MeshLibrary::LayerGroup& group = getCurrentGroup();

    int scratchChannel = group.layers[group.current].props->scratchChan;
    panelControls->setScratchSelector(scratchChannel);
}

bool Spectrum3D::validateScratchChannels() {
    panelControls->populateScratchSelector();

    int numScratchChannels = meshLib->getGroup(LayerGroups::GroupScratch).size();
    bool changed = false;

    MeshLibrary::LayerGroup& spectGroup = meshLib->getGroup(LayerGroups::GroupSpect);
    MeshLibrary::LayerGroup& phaseGroup = meshLib->getGroup(LayerGroups::GroupPhase);

    for (int i = 0; i < spectGroup.size(); ++i) {
        MeshLibrary::Layer& layer = spectGroup[i];

        if (layer.props->scratchChan >= numScratchChannels) {
            layer.props->scratchChan = CommonEnums::Null;
            changed = true;
        }
    }

    for (int i = 0; i < phaseGroup.size(); ++i) {
        MeshLibrary::Layer& layer = phaseGroup[i];

        if (layer.props->scratchChan >= numScratchChannels) {
            layer.props->scratchChan = CommonEnums::Null;
            changed = true;
        }
    }

    updateScratchComboBox();

    return changed;
}

void Spectrum3D::sliderValueChanged(Slider* slider) {
    MeshLibrary::Properties* props = getCurrentProperties();

    if (slider == dynRangeKnob) {
        props->range = slider->getValue();
    } else if (slider == layerPan) {
        props->pan = slider->getValue();
    }
}

int Spectrum3D::getLayerScratchChannel() {
    return getCurrentProperties()->scratchChan;
}


int Spectrum3D::getNumActiveLayers() {
    MeshLibrary::LayerGroup& group = getCurrentGroup();
    int numActiveLayers = 0;

    for (int i = 0; i < (int) group.size(); ++i) {
        if (group[i].props->active && group[i].mesh->hasEnoughCubesForCrossSection())
            ++numActiveLayers;
    }

    return numActiveLayers;
}

void Spectrum3D::setLayerParameterSmoothing(int voiceIndex, bool smooth) {
    MeshLibrary::LayerGroup& spectGroup = meshLib->getGroup(LayerGroups::GroupSpect);
    MeshLibrary::LayerGroup& phaseGroup = meshLib->getGroup(LayerGroups::GroupPhase);

    for (int i = 0; i < spectGroup.size(); ++i) {
        MeshLibrary::Layer& layer = spectGroup[i];

//		layer.props->pos[voiceIndex].red.setSmoothingActivity(smooth);
//		layer.props->pos[voiceIndex].blue.setSmoothingActivity(smooth);
    }

    for (int i = 0; i < phaseGroup.size(); ++i) {
        MeshLibrary::Layer& layer = phaseGroup[i];

//		layer.props->pos[voiceIndex].red.setSmoothingActivity(smooth);
//		layer.props->pos[voiceIndex].blue.setSmoothingActivity(smooth);
    }
}

void Spectrum3D::updateSmoothParametersToTarget(int voiceIndex) {
    MeshLibrary::LayerGroup& spectGroup = meshLib->getGroup(LayerGroups::GroupSpect);
    MeshLibrary::LayerGroup& phaseGroup = meshLib->getGroup(LayerGroups::GroupPhase);

    for (int i = 0; i < spectGroup.size(); ++i) {
        MeshLibrary::Layer& layer = spectGroup[i];

//		layer.props->pos[voiceIndex].red.updateToTarget();
//		layer.props->pos[voiceIndex].blue.updateToTarget();
    }

    for (int i = 0; i < phaseGroup.size(); ++i) {
        MeshLibrary::Layer& layer = phaseGroup[i];

//		layer.props->pos[voiceIndex].red.updateToTarget();
//		layer.props->pos[voiceIndex].blue.updateToTarget();
    }
}

void Spectrum3D::updateLayerSmoothedParameters(int voiceIndex, int deltaSamples) {
    MeshLibrary::LayerGroup& spectGroup = meshLib->getGroup(LayerGroups::GroupSpect);
    MeshLibrary::LayerGroup& phaseGroup = meshLib->getGroup(LayerGroups::GroupPhase);

    for (int i = 0; i < spectGroup.size(); ++i) {
        MeshLibrary::Layer& layer = spectGroup[i];

//		layer.props->pos[voiceIndex].red.update(deltaSamples);
//		layer.props->pos[voiceIndex].blue.update(deltaSamples);
    }

    for (int i = 0; i < phaseGroup.size(); ++i) {
        MeshLibrary::Layer& layer = phaseGroup[i];

//		layer.props->pos[voiceIndex].red.update(deltaSamples);
//		layer.props->pos[voiceIndex].blue.update(deltaSamples);
    }
}

bool Spectrum3D::haveAnyValidLayers(bool isMags, bool haveAnyValidTimeLayers) {
    MeshLibrary::LayerGroup& group = getCurrentGroup();

    for (int i = 0; i < group.size(); ++i) {
        MeshLibrary::Layer layer = group.layers[i];

        if (layer.props->active && layer.mesh->hasEnoughCubesForCrossSection() &&
                (haveAnyValidTimeLayers || ! isMags || layer.props->mode == Spectrum3D::Additive))
            return true;
    }

    return false;
}

void Spectrum3D::enablementsChanged() {
    bool haveTime = meshLib->hasAnyValidLayers(LayerGroups::GroupTime);

    phaseIcon.setPowered(haveAnyValidLayers(false, haveTime));
    magsIcon.setPowered(haveAnyValidLayers(true, haveTime));
}

Component* Spectrum3D::getComponent(int which) {
    switch (which) {
        case CycleTour::TargDomains: 		return modeCO.get();
        case CycleTour::TargLayerMode: 		return operCO.get();
        case CycleTour::TargPan:			return layerPan;
        case CycleTour::TargRange:			return dynRangeKnob;
        case CycleTour::TargLayerEnable: 	return &panelControls->enableCurrent;
        case CycleTour::TargScratchBox:		return &panelControls->scratchSelector;
        case CycleTour::TargLayerAdder:		return &panelControls->addRemover;
        case CycleTour::TargLayerMover:		return &panelControls->upDownMover;
        case CycleTour::TargLayerSlct:		return panelControls->layerSelector.get();
        case CycleTour::TargMeshSelector:	return meshSelector.get();
        default: break;
    }

    return nullptr;
}

void Spectrum3D::triggerButton(int button) {
    switch (button) {
        case CycleTour::IdBttnEnable: 			buttonClicked(&panelControls->enableCurrent); 		break;
        case CycleTour::IdBttnAdd: 	 			buttonClicked(&panelControls->addRemover.add); 		break;
        case CycleTour::IdBttnRemove: 			buttonClicked(&panelControls->addRemover.remove); 	break;
        case CycleTour::IdBttnMoveUp: 			buttonClicked(&panelControls->upDownMover.up); 		break;
        case CycleTour::IdBttnMoveDown: 		buttonClicked(&panelControls->upDownMover.down); 	break;
        case CycleTour::IdBttnModeAdditive: 	buttonClicked(&additiveIcon); 						break;
        case CycleTour::IdBttnModeFilter: 		buttonClicked(&subtractiveIcon); 					break;
        default: break;
    }
}

Buffer<float> Spectrum3D::getColumnArray() {
    return getObj(VisualDsp).getFreqArray();
}

const vector <Column>& Spectrum3D::getColumns() {
    return getObj(VisualDsp).getFreqColumns();
}

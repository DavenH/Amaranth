#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <Curve/GuideCurveProvider.h>
#include <Design/Updating/Updater.h>
#include <UI/IConsole.h>

#include "GuideCurvePanel.h"

#include <Util/ScopedFunction.h>

#include "../Panels/VertexPropertiesPanel.h"
#include "../Widgets/Controls/LayerAddRemover.h"
#include "../Widgets/Controls/MeshSelector.h"
#include "../../App/CycleTour.h"
#include "../../Audio/SynthAudioSource.h"
#include "../CycleDefs.h"
#include "../../Util/CycleEnums.h"

GuideCurvePanel::GuideCurvePanel(SingletonRepo* repo) :
        EffectPanel	(repo, "GuideCurvePanel", false)
    ,	LayerSelectionClient(repo)
    ,	SingletonAccessor(repo, "GuideCurvePanel")
    ,	noise		(repo, "noise", "Noise Level")
    ,	vertOffset	(repo, "offset", "Random DC Offset Range")
    ,	phaseOffset	(repo, "phase", "Random Phase Range")
    ,	constMemory	(tableSize * 2)
    ,	random		(Time::currentTimeMillis())
{
}

void GuideCurvePanel::init() {
    EffectPanel::init();

    meshLib                = &getObj(MeshLibrary);
    updateSource           = UpdateSources::SourceGuideCurve;
    layerType              = LayerGroups::GroupGuideCurve;
    nameCornerPos          = juce::Point<float>(-25, 5);
    curveIsBipolar         = true;
    vertsAreWaveApplicable = true;

    float pad         = getConstant(GuideCurvePadding);
    bgPaddingRight    = pad;
    bgPaddingLeft     = pad;
    zoomPanel->rect.w = 1.f - pad;
    zoomPanel->rect.x = 0.5f * pad;
//	zoomPanel->setZoomContext(ZoomContext(this, nullptr, true, false));

    noise.addListener		(this);
    noise.setRange			(0, 1, 0.025);
    noise.setValue			(0, dontSendNotification);

    vertOffset.addListener	(this);
    vertOffset.setRange		(0, 1, 0.025);
    vertOffset.setValue		(0, dontSendNotification);
    vertOffset.setColour	(Colour(168, 19, 116));

    phaseOffset.addListener	(this);
    phaseOffset.setRange	(0, 1, 0.025);
    phaseOffset.setValue	(0, dontSendNotification);
    phaseOffset.setColour	(Colour(77, 143, 229));

    samplingInterval = (1.f - 2 * getConstant(GuideCurvePadding)) / float(tableSize - 1);

    unsigned int seed(Time::currentTimeMillis());
    noiseArray = constMemory.place(tableSize);
    noiseArray.rand(seed).mul(0.5f);
    phaseMoveBuffer = constMemory.place(tableSize);

    createNameImage("Guide Curves");
    dynMemory.resize(tableSize * 4);
    guideTables.emplace_back(this, 0, 0, 0, 0, 0);
    setGuideBuffers();

    rasterizer->cleanUp();
    rasterizer->setMesh(meshLib->getCurrentMesh(LayerGroups::GroupGuideCurve));
    rasterizer->performUpdate(Update);
    rasterizer->setGuideCurveProvider(this);

    meshSelector = std::make_unique<MeshSelector<Mesh>>(repo, this, "gc", true, false, true);

    panelControls = std::make_unique<PanelControls>(repo, this, this, this, "Guide Curve");
    panelControls->addLayerItems(this, false, true);
    panelControls->addSlider(&noise);
    panelControls->addSlider(&vertOffset);
    panelControls->addSlider(&phaseOffset);
    panelControls->addMeshSelector(meshSelector.get());
}

int GuideCurvePanel::getNumGuides() {
    return meshLib->getLayerGroup(LayerGroups::GroupGuideCurve).size();
}

void GuideCurvePanel::updateDspSync() {
    rasterizeTable();
}

void GuideCurvePanel::rasterizeAllTables() {
    enterClientLock();

    MeshLibrary::LayerGroup& guideCurveGroup = meshLib->getLayerGroup(LayerGroups::GroupGuideCurve);

    for(int i = 0; i < guideCurveGroup.size(); ++i) {
        Mesh* mesh = guideCurveGroup.layers[i].mesh;

        rasterizer->cleanUp();
        rasterizer->setMesh(mesh);
        rasterizer->performUpdate(Update);
        rasterizer->sampleWithInterval(guideTables[i].table,
                                       samplingInterval, (float) getRealConstant(GuideCurvePadding));

        guideTables[i].table.add(-0.5f);
    }

    rasterizer->cleanUp();
    rasterizer->setMesh(guideCurveGroup.getCurrentMesh());
    rasterizer->performUpdate(Update);

    exitClientLock();
}

void GuideCurvePanel::rasterizeTable() {
    ScopedLock sl(renderLock);

    int currentLayer = meshLib->getLayerGroup(LayerGroups::GroupGuideCurve).current;

    GuideCurveProps& props = guideTables[currentLayer];

    rasterizer->performUpdate(Update);
    if (rasterizer->hasEnoughCubesForCrossSection()) {
        rasterizer->sampleWithInterval(props.table, samplingInterval, (float) getRealConstant(GuideCurvePadding));

        props.table.add(-0.5f);
    } else {
        props.table.set(0.5f);
    }
}

void GuideCurvePanel::preDraw()
{
    float padding	= getRealConstant(GuideCurvePadding);
    int left 		= 0;
    int right 		= getWidth();
    int innerRight 	= sx(1 - padding);
    int innerLeft 	= sx(padding);
    int bottom 		= 0;
    int top 		= getHeight();

    gfx->setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
    gfx->fillRect(left, top, innerLeft, bottom, false);
    gfx->fillRect(right, top, innerRight, bottom, false);

    gfx->disableSmoothing();
    gfx->setCurrentColour(0.2f, 0.2f, 0.2f, 0.5f);

    gfx->drawLine(innerLeft, top, innerLeft, bottom, false);
    gfx->drawLine(innerRight, top, innerRight, bottom, false);
    gfx->enableSmoothing();

    int currentLayer = meshLib->getLayerGroup(LayerGroups::GroupGuideCurve).current;

    GuideCurveProps& props = guideTables[currentLayer];

    float rectLeft = padding;
    float rectRight = 1 - padding;
    float rectSize = 1 - 2 * padding;

    gfx->setCurrentColour(0.7f, 0.55f, 0.18f, 0.17f);
    gfx->fillRect(rectLeft, 0.5 + 0.5 * props.noiseLevel, rectRight, 0.5f - 0.5f * props.noiseLevel, true);

    gfx->setCurrentColour(0.7f, 0.08f, 0.5f, 0.17f);
    gfx->fillRect(rectLeft, 0.5 + 0.5 * props.vertOffsetLevel, rectRight, 0.5f - 0.5f * props.vertOffsetLevel, true);

    gfx->setCurrentColour(0.3f, 0.6f, 0.9f, 0.17f);
    gfx->fillRect(0.5 - 0.5 * rectSize * props.phaseOffsetLevel, 0, 0.5 + 0.5 * rectSize * props.phaseOffsetLevel, 1.f, true);
}

bool GuideCurvePanel::isEffectEnabled() const {
    return true;
}

void GuideCurvePanel::buttonClicked(Button* button) {
    if(button == &panelControls->addRemover.add ||
       button == &panelControls->addRemover.remove)
    {
        bool forceUpdate = false;
        bool globalUpdate = false;

        {
            ScopedLock lock(getObj(SynthAudioSource).getLock());

            if(button == &panelControls->addRemover.add) {
                addNewLayer(false);
            } else if (button == &panelControls->addRemover.remove) {
                int index = meshLib->getLayerGroup(LayerGroups::GroupGuideCurve).current;

                {
                    ScopedLock lock(getObj(SynthAudioSource).getLock());
                    if(index > 0)
                        guideTables.erase(guideTables.begin() + index);

                    setGuideBuffers();

                    meshLib->removeLayerKeepingOne(LayerGroups::GroupGuideCurve, index);
                    globalUpdate |= meshLib->layerRemoved(LayerGroups::GroupGuideCurve, index) || index == 0;
                    meshLib->layerChanged(LayerGroups::GroupGuideCurve, index);
                }

                repaint();
                forceUpdate = index == 0;
            }

            rasterizeAllTables();
        }

        getObj(VertexPropertiesPanel).updateComboBoxes();
        panelControls->layerSelector->refresh(forceUpdate);

        if(globalUpdate) {
            getObj(Updater).update(UpdateSources::SourceWaveform3D);
        }
    }
}

void GuideCurvePanel::addNewLayer(bool update) {
    meshLib->addLayer(LayerGroups::GroupGuideCurve);

    // incremented by the above
    int currentLayer = meshLib->getLayerGroup(LayerGroups::GroupGuideCurve).current;

    {
        enterClientLock();
        guideTables.emplace_back(this, currentLayer, 0, 0, 0, random.nextInt(tableSize));
        exitClientLock();
    }

    setGuideBuffers();

    if (update) {
        rasterizeAllTables();
        getObj(VertexPropertiesPanel).updateComboBoxes();
        panelControls->layerSelector->refresh();

        layerChanged();
    }
}

void GuideCurvePanel::panelResized() {
    Panel::panelResized();

//	Rectangle<int> bounds = comp->getLocalBounds();
//	nameTexA->rect = bounds.removeFromRight(nameImage.getWidth()).removeFromTop(nameImage.getHeight()).toFloat();


//	controls.resized();
}

void GuideCurvePanel::sliderDragEnded(Slider* slider) {
    // this usually gets updated in updateDspSync, via postUpdateMessage,
    // but when we're no updating in realtime there are none of these calls
    if (!getSetting(UpdateGfxRealtime)) {
        rasterizeTable();
    }

    triggerRestoreUpdate();
}

void GuideCurvePanel::sliderDragStarted(Slider* slider) {
    if (getSetting(UpdateGfxRealtime)) {
        triggerReduceUpdate();
    }
}

void GuideCurvePanel::sliderValueChanged(Slider* slider) {
    progressMark

    int currentLayer = meshLib->getLayerGroup(LayerGroups::GroupGuideCurve).current;

    GuideCurveProps& props = guideTables[currentLayer];

    if (slider == &noise) {
        props.noiseLevel = powf(noise.getValue(), 2);
    } else if (slider == &vertOffset) {
        props.vertOffsetLevel = powf(vertOffset.getValue(), 2);
    } else if (slider == &phaseOffset) {
        props.phaseOffsetLevel = powf(phaseOffset.getValue(), 2);
    }

    if (getSetting(UpdateGfxRealtime)) {
        postUpdateMessage();
    }
}

void GuideCurvePanel::layerChanged() {
    progressMark

    rasterizer->cleanUp();
    rasterizer->setMesh(meshLib->getCurrentMesh(layerType));
    clearSelectedAndCurrent();

    meshLib->layerChanged(LayerGroups::GroupGuideCurve, -1);
    updateKnobsImplicit();
    getRasterizer()->performUpdate(Update);
    performUpdate(Update);
    // this should only trigger updates to self if before envelope viewstage
    //	postUpdateMessage();
}

void GuideCurvePanel::updateKnobsImplicit() {
    int currentLayer = meshLib->getLayerGroup(layerType).current;

    if(! isPositiveAndBelow(currentLayer, (int) guideTables.size())) {
        return;
    }

    GuideCurveProps& props = guideTables[currentLayer];
    float noiseValue = sqrtf(props.noiseLevel);
    noise.setValue(noiseValue, dontSendNotification);

    float offsetValue = sqrtf(props.vertOffsetLevel);
    vertOffset.setValue(offsetValue, dontSendNotification);

    float phaseValue = sqrtf(props.phaseOffsetLevel);
    phaseOffset.setValue(phaseValue, dontSendNotification);
}

void GuideCurvePanel::setCurrentMesh(Mesh* mesh) {
    meshLib->setCurrentMesh(layerType, mesh);

    clearSelectedAndCurrent();
    setMeshAndUpdate(mesh);
    rasterizeTable();

    triggerRefreshUpdate();
}

void GuideCurvePanel::previewMesh(Mesh* mesh) {
    ScopedLock sl(renderLock);

    setMeshAndUpdate(mesh);
}

void GuideCurvePanel::previewMeshEnded(Mesh* oldMesh) {
    ScopedLock sl(renderLock);

    setMeshAndUpdate(oldMesh);
}

void GuideCurvePanel::setMeshAndUpdate(Mesh* mesh) {
    mesh->updateToVersion(ProjectInfo::versionNumber);

    rasterizer->cleanUp();
    rasterizer->setMesh(mesh);
    rasterizer->performUpdate(Update);
    repaint();
}

void GuideCurvePanel::enterClientLock() {
    getObj(SynthAudioSource).getLock().enter();
    renderLock.enter();
}

void GuideCurvePanel::exitClientLock() {
    renderLock.exit();
    getObj(SynthAudioSource).getLock().exit();
}

Mesh* GuideCurvePanel::getCurrentMesh() {
    return Interactor::getMesh();
}

bool GuideCurvePanel::setGuideBuffers() {
    ScopedLambda locker(
        [this]{enterClientLock();},
        [this]{exitClientLock();});

    bool changed = dynMemory.ensureSize(guideTables.size() * tableSize);

    for (auto& guideTable : guideTables) {
        guideTable.table = dynMemory.place(tableSize);
    }

    return changed;
}

bool GuideCurvePanel::readXML(const XmlElement* element) {
    XmlElement* guideCurveElem = element->getChildByName("GuideCurveProps");

    if (guideCurveElem == nullptr) {
        guideCurveElem = element->getChildByName("DeformerProps");
    }

    guideTables.clear();
    int index = 0;

    int numMeshes = meshLib->getLayerGroup(layerType).size();

    if (guideCurveElem != nullptr) {
        for(auto propsElem : guideCurveElem->getChildWithTagNameIterator("Properties")) {
            if (guideTables.size() >= numMeshes) {
                break;
            }

            float noiseLevel 	= propsElem->getDoubleAttribute ("noiseLevel",  0);
            float offsetLevel 	= propsElem->getDoubleAttribute ("offsetLevel", 0);
            float phaseLevel 	= propsElem->getDoubleAttribute ("phaseLevel",  0);
            int seed			= propsElem->getIntAttribute	("noiseSeed",  -1);

            if(seed == -1) {
                seed = random.nextInt(tableSize);
            }

            guideTables.emplace_back(this, index++, noiseLevel, offsetLevel, phaseLevel, seed);
        }
    }

    while ((int) guideTables.size() < numMeshes) {
        guideTables.emplace_back(this, index++, 0, 0, 0, random.nextInt(tableSize));
    }

    panelControls->layerSelector->reset();

    updateKnobsImplicit();
    setGuideBuffers();
    rasterizeAllTables();

    return true;
}

int GuideCurvePanel::getTableDensity(int index) {
    MeshLibrary::Layer& layer = meshLib->getLayer(layerType, index);

    if (layer.mesh == nullptr) {
        return 0;
    }

    return layer.mesh->getNumVerts();
}

void GuideCurvePanel::writeXML(XmlElement* element) const
{
    auto* guideCurveElem = new XmlElement("GuideCurveProps");

    for (const auto& props: guideTables) {
        auto* propsElem 	= new XmlElement("Properties");
        propsElem->setAttribute("noiseLevel", 	props.noiseLevel);
        propsElem->setAttribute("offsetLevel", 	props.vertOffsetLevel);
        propsElem->setAttribute("phaseLevel", 	props.phaseOffsetLevel);
        propsElem->setAttribute("noiseSeed", 	props.seed);

        guideCurveElem->addChildElement(propsElem);
    }

    element->addChildElement(guideCurveElem);
}

void GuideCurvePanel::sampleDownAddNoise(int index,
                                       Buffer<float> dest,
                                       const NoiseContext& context) {
    int length = dest.size();

    GuideCurveProps& props 	= guideTables[index];
    Buffer<Float32> table = props.table;

    dest.downsampleFrom(table);

    length = dest.size();

    if (props.phaseOffsetLevel > 0) {
        int phaseOffset = (context.phaseOffset & tableModulo - tableSize / 2) * props.phaseOffsetLevel;

        dest.withPhase(phaseOffset % length, phaseMoveBuffer.withSize(length));
    }

    if (props.noiseLevel > 0.f) {
        int noiseOffset = (props.seed + context.noiseSeed) & tableModulo;
        int elemsToCopy = jmin(length, noiseArray.size() - noiseOffset);

        dest.addProduct(noiseArray.section(noiseOffset, elemsToCopy), props.noiseLevel);

        if (length - elemsToCopy > 0) {
            dest.offset(elemsToCopy).addProduct(noiseArray, props.noiseLevel);
        }
    }

    if (props.vertOffsetLevel > 0.f) {
        int offset = (props.seed + context.vertOffset) & tableModulo;
        dest.add(props.vertOffsetLevel * noiseArray[offset]);
    }
}

void GuideCurvePanel::reset() {
    //	panelControls->resetLayerBox();

    panelControls->layerSelector->reset();
}

void GuideCurvePanel::doubleMesh() {
    getCurrentMesh()->twin(getConstant(GuideCurvePadding), getConstant(GuideCurvePadding));
    postUpdateMessage();
}

juce::Component* GuideCurvePanel::getComponent(int which) {
    switch (which) {
        case CycleTour::TargDfrmNoise: 		return &noise;
        case CycleTour::TargDfrmOffset: 	return &vertOffset;
        case CycleTour::TargDfrmPhase: 		return &phaseOffset;
        case CycleTour::TargDfrmMeshSlct: 	return meshSelector.get();
        case CycleTour::TargDfrmLayerAdd:	return &panelControls->addRemover;
        case CycleTour::TargDfrmLayerSlct:	return panelControls->layerSelector.get();
        default:
            break;
    }

    return nullptr;
}

void GuideCurvePanel::triggerButton(int id) {
    switch (id) {
        case CycleTour::IdBttnAdd:      buttonClicked(&panelControls->addRemover.add);    break;
        case CycleTour::IdBttnRemove:   buttonClicked(&panelControls->addRemover.remove); break;
        default:
            break;
    }
}

void GuideCurvePanel::showCoordinates() {
    float invSize = 1.f / float(1.f - 2.f * getConstant(GuideCurvePadding));
    float xformX  = (state.currentMouse.x - getConstant(GuideCurvePadding)) * invSize;

    String message = String(xformX, 2) + ", " + String(state.currentMouse.y, 2);
    showConsoleMsg(message);
}

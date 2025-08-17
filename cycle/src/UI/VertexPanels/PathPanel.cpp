#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <Design/Updating/Updater.h>
#include <UI/IConsole.h>

#include "PathPanel.h"

#include <Util/ScopedFunction.h>

#include "../Panels/VertexPropertiesPanel.h"
#include "../Widgets/Controls/LayerAddRemover.h"
#include "../Widgets/Controls/MeshSelector.h"
#include "../../App/CycleTour.h"
#include "../../Audio/SynthAudioSource.h"
#include "../CycleDefs.h"
#include "../../Util/CycleEnums.h"

PathPanel::PathPanel(SingletonRepo* repo) :
        EffectPanel	(repo, "PathPanel", false)
    ,	LayerSelectionClient(repo)
    ,	SingletonAccessor(repo, "PathPanel")
    ,	noise		(repo, "noise", "Noise Level")
    ,	vertOffset	(repo, "offset", "Random DC Offset Range")
    ,	phaseOffset	(repo, "phase", "Random Phase Range")
    ,	constMemory	(tableSize * 2)
    ,	random		(Time::currentTimeMillis())
{
}

void PathPanel::init() {
    meshLib                = &getObj(MeshLibrary);
    updateSource           = UpdateSources::SourcePath;
    layerType              = LayerGroups::GroupPath;
    nameCornerPos          = juce::Point<float>(-25, 5);
    curveIsBipolar         = true;
    vertsAreWaveApplicable = true;

    float pad         = getConstant(PathPadding);
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

    samplingInterval = (1.f - 2 * getConstant(PathPadding)) / float(tableSize - 1);

    unsigned int seed(Time::currentTimeMillis());
    noiseArray = constMemory.place(tableSize);
    noiseArray.rand(seed).mul(0.5f);
    phaseMoveBuffer = constMemory.place(tableSize);

    createNameImage("Paths");
    dynMemory.resize(tableSize * 4);
    curvePathTables.emplace_back(this, 0, 0, 0, 0, 0);
    setGuideBuffers();

    rasterizer->cleanUp();
    rasterizer->setMesh(meshLib->getCurrentMesh(LayerGroups::GroupPath));
    rasterizer->performUpdate(Update);
    rasterizer->setPath(this);	// lol

    meshSelector = std::make_unique<MeshSelector<Mesh>>(repo, this, "df", true, false, true);

    panelControls = std::make_unique<PanelControls>(repo, this, this, this, "Path");
    panelControls->addLayerItems(this, false, true);
    panelControls->addSlider(&noise);
    panelControls->addSlider(&vertOffset);
    panelControls->addSlider(&phaseOffset);
    panelControls->addMeshSelector(meshSelector.get());
}

int PathPanel::getNumGuides() {
    return meshLib->getLayerGroup(LayerGroups::GroupPath).size();
}

void PathPanel::updateDspSync() {
    rasterizeTable();
}

void PathPanel::rasterizeAllTables() {
    enterClientLock();

    MeshLibrary::LayerGroup& pathGroup = meshLib->getLayerGroup(LayerGroups::GroupPath);

    for(int i = 0; i < pathGroup.size(); ++i) {
        Mesh* mesh = pathGroup.layers[i].mesh;

        rasterizer->cleanUp();
        rasterizer->setMesh(mesh);
        rasterizer->performUpdate(Update);
        rasterizer->sampleWithInterval(curvePathTables[i].table,
                                       samplingInterval, (float) getRealConstant(PathPadding));

        curvePathTables[i].table.add(-0.5f);
    }

    rasterizer->cleanUp();
    rasterizer->setMesh(pathGroup.getCurrentMesh());
    rasterizer->performUpdate(Update);

    exitClientLock();
}

void PathPanel::rasterizeTable() {
    ScopedLock sl(renderLock);

    int currentLayer = meshLib->getLayerGroup(LayerGroups::GroupPath).current;

    CurvePathProps& props = curvePathTables[currentLayer];

    rasterizer->performUpdate(Update);
    if (rasterizer->hasEnoughCubesForCrossSection()) {
        rasterizer->sampleWithInterval(props.table, samplingInterval, (float) getRealConstant(PathPadding));

        props.table.add(-0.5f);
    } else {
        props.table.set(0.5f);
    }
}

void PathPanel::preDraw()
{
    float padding	= getRealConstant(PathPadding);
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

    int currentLayer = meshLib->getLayerGroup(LayerGroups::GroupPath).current;

    CurvePathProps& props = curvePathTables[currentLayer];

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

bool PathPanel::isEffectEnabled() const {
    return true;
}

void PathPanel::buttonClicked(Button* button) {
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
                int index = meshLib->getLayerGroup(LayerGroups::GroupPath).current;

                {
                    ScopedLock lock(getObj(SynthAudioSource).getLock());
                    if(index > 0)
                        curvePathTables.erase(curvePathTables.begin() + index);

                    setGuideBuffers();

                    meshLib->removeLayerKeepingOne(LayerGroups::GroupPath, index);
                    globalUpdate |= meshLib->layerRemoved(LayerGroups::GroupPath, index) || index == 0;
                    meshLib->layerChanged(LayerGroups::GroupPath, index);
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

void PathPanel::addNewLayer(bool update) {
    meshLib->addLayer(LayerGroups::GroupPath);

    // incremented by the above
    int currentLayer = meshLib->getLayerGroup(LayerGroups::GroupPath).current;

    {
        enterClientLock();
        curvePathTables.emplace_back(this, currentLayer, 0, 0, 0, random.nextInt(tableSize));
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

void PathPanel::panelResized() {
    Panel::panelResized();

//	Rectangle<int> bounds = comp->getLocalBounds();
//	nameTexA->rect = bounds.removeFromRight(nameImage.getWidth()).removeFromTop(nameImage.getHeight()).toFloat();


//	controls.resized();
}

void PathPanel::sliderDragEnded(Slider* slider) {
    // this usually gets updated in updateDspSync, via postUpdateMessage,
    // but when we're no updating in realtime there are none of these calls
    if (!getSetting(UpdateGfxRealtime)) {
        rasterizeTable();
    }

    triggerRestoreUpdate();
}

void PathPanel::sliderDragStarted(Slider* slider) {
    if (getSetting(UpdateGfxRealtime)) {
        triggerReduceUpdate();
    }
}

void PathPanel::sliderValueChanged(Slider* slider) {
    progressMark

    int currentLayer = meshLib->getLayerGroup(LayerGroups::GroupPath).current;

    CurvePathProps& props = curvePathTables[currentLayer];

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

void PathPanel::layerChanged() {
    progressMark

    rasterizer->cleanUp();
    rasterizer->setMesh(meshLib->getCurrentMesh(layerType));
    clearSelectedAndCurrent();

    meshLib->layerChanged(LayerGroups::GroupPath, -1);
    updateKnobsImplicit();
    getRasterizer()->performUpdate(Update);
    performUpdate(Update);
    // this should only trigger updates to self if before envelope viewstage
    //	postUpdateMessage();
}

void PathPanel::updateKnobsImplicit() {
    int currentLayer = meshLib->getLayerGroup(layerType).current;

    if(! isPositiveAndBelow(currentLayer, (int) curvePathTables.size())) {
        return;
    }

    CurvePathProps& props = curvePathTables[currentLayer];
    float noiseValue = sqrtf(props.noiseLevel);
    noise.setValue(noiseValue, dontSendNotification);

    float offsetValue = sqrtf(props.vertOffsetLevel);
    vertOffset.setValue(offsetValue, dontSendNotification);

    float phaseValue = sqrtf(props.phaseOffsetLevel);
    phaseOffset.setValue(phaseValue, dontSendNotification);
}

void PathPanel::setCurrentMesh(Mesh* mesh) {
    meshLib->setCurrentMesh(layerType, mesh);

    clearSelectedAndCurrent();
    setMeshAndUpdate(mesh);
    rasterizeTable();

    triggerRefreshUpdate();
}

void PathPanel::previewMesh(Mesh* mesh) {
    ScopedLock sl(renderLock);

    setMeshAndUpdate(mesh);
}

void PathPanel::previewMeshEnded(Mesh* oldMesh) {
    ScopedLock sl(renderLock);

    setMeshAndUpdate(oldMesh);
}

void PathPanel::setMeshAndUpdate(Mesh* mesh) {
    mesh->updateToVersion(ProjectInfo::versionNumber);

    rasterizer->cleanUp();
    rasterizer->setMesh(mesh);
    rasterizer->performUpdate(Update);
    repaint();
}

void PathPanel::enterClientLock() {
    getObj(SynthAudioSource).getLock().enter();
    renderLock.enter();
}

void PathPanel::exitClientLock() {
    renderLock.exit();
    getObj(SynthAudioSource).getLock().exit();
}

Mesh* PathPanel::getCurrentMesh() {
    return Interactor::getMesh();
}

bool PathPanel::setGuideBuffers() {
    ScopedLambda locker(
        [this]{enterClientLock();},
        [this]{exitClientLock();});

    bool changed = dynMemory.ensureSize(curvePathTables.size() * tableSize);

    for (auto& guideTable : curvePathTables) {
        guideTable.table = dynMemory.place(tableSize);
    }

    return changed;
}

bool PathPanel::readXML(const XmlElement* element) {
    XmlElement* pathElem = element->getChildByName("PathProps");

    curvePathTables.clear();
    int index = 0;

    int numMeshes = meshLib->getLayerGroup(layerType).size();

    if (pathElem != nullptr) {
        for(auto propsElem : pathElem->getChildWithTagNameIterator("Properties")) {
            if (curvePathTables.size() >= numMeshes) {
                break;
            }

            float noiseLevel 	= propsElem->getDoubleAttribute ("noiseLevel",  0);
            float offsetLevel 	= propsElem->getDoubleAttribute ("offsetLevel", 0);
            float phaseLevel 	= propsElem->getDoubleAttribute ("phaseLevel",  0);
            int seed			= propsElem->getIntAttribute	("noiseSeed",  -1);

            if(seed == -1) {
                seed = random.nextInt(tableSize);
            }

            curvePathTables.emplace_back(this, index++, noiseLevel, offsetLevel, phaseLevel, seed);
        }
    }

    while ((int) curvePathTables.size() < numMeshes) {
        curvePathTables.emplace_back(this, index++, 0, 0, 0, random.nextInt(tableSize));
    }

    panelControls->layerSelector->reset();

    updateKnobsImplicit();
    setGuideBuffers();
    rasterizeAllTables();

    return true;
}

int PathPanel::getTableDensity(int index) {
    MeshLibrary::Layer& layer = meshLib->getLayer(layerType, index);

    if (layer.mesh == nullptr) {
        return 0;
    }

    return layer.mesh->getNumVerts();
}

void PathPanel::writeXML(XmlElement* element) const
{
    auto* pathElem = new XmlElement("PathProps");

    for (const auto& props: curvePathTables) {
        auto* propsElem 	= new XmlElement("Properties");
        propsElem->setAttribute("noiseLevel", 	props.noiseLevel);
        propsElem->setAttribute("offsetLevel", 	props.vertOffsetLevel);
        propsElem->setAttribute("phaseLevel", 	props.phaseOffsetLevel);
        propsElem->setAttribute("noiseSeed", 	props.seed);

        pathElem->addChildElement(propsElem);
    }

    element->addChildElement(pathElem);
}

void PathPanel::sampleDownAddNoise(int index,
                                   Buffer<float> dest,
                                   const NoiseContext& context) {
    int length = dest.size();

    CurvePathProps& props 	= curvePathTables[index];
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

void PathPanel::reset() {
    //	panelControls->resetLayerBox();

    panelControls->layerSelector->reset();
}

void PathPanel::doubleMesh() {
    getCurrentMesh()->twin(getConstant(PathPadding), getConstant(PathPadding));
    postUpdateMessage();
}

juce::Component* PathPanel::getComponent(int which) {
    switch (which) {
        case CycleTour::TargPathNoise: 		return &noise;
        case CycleTour::TargPathOffset: 	return &vertOffset;
        case CycleTour::TargPathPhase: 		return &phaseOffset;
        case CycleTour::TargPathMeshSlct: 	return meshSelector.get();
        case CycleTour::TargPathLayerAdd:	return &panelControls->addRemover;
        case CycleTour::TargPathLayerSlct:	return panelControls->layerSelector.get();
        default:
            break;
    }

    return nullptr;
}

void PathPanel::triggerButton(int id) {
    switch (id) {
        case CycleTour::IdBttnAdd:      buttonClicked(&panelControls->addRemover.add);    break;
        case CycleTour::IdBttnRemove:   buttonClicked(&panelControls->addRemover.remove); break;
        default:
            break;
    }
}

void PathPanel::showCoordinates() {
    float invSize = 1.f / float(1.f - 2.f * getConstant(PathPadding));
    float xformX  = (state.currentMouse.x - getConstant(PathPadding)) * invSize;

    String message = String(xformX, 2) + ", " + String(state.currentMouse.y, 2);
    showConsoleMsg(message);
}

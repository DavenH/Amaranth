#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <Curve/IDeformer.h>
#include <Design/Updating/Updater.h>
#include <ipp.h>
#include <Thread/LockTracer.h>
#include <UI/IConsole.h>
#include <UI/MiscGraphics.h>
#include <UI/Panels/Texture.h>
#include <UI/Widgets/DynamicLabel.h>

#include "DeformerPanel.h"
#include "../Panels/VertexPropertiesPanel.h"
#include "../Widgets/Controls/ControlsPanel.h"
#include "../Widgets/Controls/LayerAddRemover.h"
#include "../Widgets/Controls/MeshSelector.h"
#include "../Widgets/Controls/Spacers.h"
#include "../../App/CycleTour.h"
#include "../../App/Directories.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../CycleDefs.h"
#include "../../Util/CycleEnums.h"


DeformerPanel::DeformerPanel(SingletonRepo* repo) : 
		EffectPanel	(repo, "DeformerPanel", false)
	,	LayerSelectionClient(repo)
	,	SingletonAccessor(repo, "DeformerPanel")
	,	noise		(repo, "noise", "Noise Level")
	,	vertOffset	(repo, "offset", "Random DC Offset Range")
	,	phaseOffset	(repo, "phase", "Random Phase Range")
	,	constMemory	(tableSize * 2)
	,	random		(Time::currentTimeMillis())
{
}


DeformerPanel::~DeformerPanel()
{
}

void DeformerPanel::init()
{
	meshLib 		= &getObj(MeshLibrary);
	updateSource 	= UpdateSources::SourceDeformer;
	layerType 		= LayerGroups::GroupDeformer;
	nameCornerPos	= Point<float>(-25, 5);

	curveIsBipolar 	= true;
	vertsAreWaveApplicable = true;

	float pad = getConstant(DeformerPadding);
	bgPaddingRight = pad;
	bgPaddingLeft = pad;
	zoomPanel->rect.w = 1.f - pad;
	zoomPanel->rect.x = 0.5f * pad;
//	zoomPanel->setZoomContext(ZoomContext(this, nullptr, true, false));

	noise.addListener		(this);
	noise.setRange			(0, 1, 0.025);
	noise.setValue			(0, dontSendNotification);

	vertOffset.addListener	(this);
	vertOffset.setRange		(0, 1, 0.025);
	vertOffset.setValue		(0, dontSendNotification);
	vertOffset.setColour	(Colour((uint8)168, (uint8)19, (uint8)116));

	phaseOffset.addListener	(this);
	phaseOffset.setRange	(0, 1, 0.025);
	phaseOffset.setValue	(0, dontSendNotification);
	phaseOffset.setColour	(Colour((uint8)77, (uint8)143, (uint8)229));

	samplingInterval = (1.f - 2 * getConstant(DeformerPadding)) / float(tableSize - 1);

	unsigned int seed(Time::currentTimeMillis());
	noiseArray = constMemory.place(tableSize);
	noiseArray.rand(seed).mul(0.5f);
	phaseMoveBuffer = constMemory.place(tableSize);

	createNameImage("Deformers");
	dynMemory.resize(tableSize * 4);
	guideTables.push_back(GuideProps(this, 0, 0, 0, 0, 0));
	setGuideBuffers();

	rasterizer->cleanUp();
	rasterizer->setMesh(meshLib->getCurrentMesh(LayerGroups::GroupDeformer));
	rasterizer->performUpdate(UpdateType::Update);
	rasterizer->setDeformer(this);	// lol

	meshSelector = new MeshSelector<Mesh>(repo, this, "df", true, false, true);

	panelControls = new PanelControls(repo, this, this, this, "Deformer");
	panelControls->addLayerItems(this, false, true);
	panelControls->addSlider(&noise);
	panelControls->addSlider(&vertOffset);
	panelControls->addSlider(&phaseOffset);
	panelControls->addMeshSelector(meshSelector);
}


int DeformerPanel::getNumGuides()
{
	return meshLib->getGroup(LayerGroups::GroupDeformer).size();
}


void DeformerPanel::updateDspSync()
{
	rasterizeTable();
}


void DeformerPanel::rasterizeAllTables()
{
	onlyBeat(return);

	enterClientLock();

	MeshLibrary::LayerGroup& dfrmGroup = meshLib->getGroup(LayerGroups::GroupDeformer);

	for(int i = 0; i < (int) dfrmGroup.size(); ++i)
	{
		Mesh* mesh = dfrmGroup.layers[i].mesh;

		rasterizer->cleanUp();
		rasterizer->setMesh(mesh);
		rasterizer->performUpdate(UpdateType::Update);
		rasterizer->sampleWithInterval(guideTables[i].table,
		                               samplingInterval, (float) getRealConstant(DeformerPadding));

		guideTables[i].table.add(-0.5f);
	}

	rasterizer->cleanUp();
	rasterizer->setMesh(dfrmGroup.getCurrentMesh());
	rasterizer->performUpdate(UpdateType::Update);

	exitClientLock();
}


void DeformerPanel::rasterizeTable()
{
	onlyBeat(return);

	ScopedLock sl(renderLock);

	int currentLayer = meshLib->getGroup(LayerGroups::GroupDeformer).current;

	GuideProps& props = guideTables[currentLayer];

	rasterizer->performUpdate(UpdateType::Update);
	if(rasterizer->hasEnoughCubesForCrossSection())
	{
		rasterizer->sampleWithInterval(props.table, samplingInterval, (float) getRealConstant(DeformerPadding));

		props.table.add(-0.5f);
	}
	else
	{
		props.table.set(0.5f);
	}
}


void DeformerPanel::preDraw()
{
	float padding	= getRealConstant(DeformerPadding);
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

	int currentLayer = meshLib->getGroup(LayerGroups::GroupDeformer).current;

	GuideProps& props = guideTables[currentLayer];

	float rectLeft = padding;
	float rectRight = 1 - padding;
	float rectSize = 1 - 2 * padding;

	gfx->setCurrentColour(0.7f, 0.55f, 0.18f, 0.17f);
	gfx->fillRect(rectLeft, 0.5 + 0.5 * props.noiseLevel, rectRight, 0.5f - 0.5f * props.noiseLevel);

	gfx->setCurrentColour(0.7f, 0.08f, 0.5f, 0.17f);
	gfx->fillRect(rectLeft, 0.5 + 0.5 * props.vertOffsetLevel, rectRight, 0.5f - 0.5f * props.vertOffsetLevel);

	gfx->setCurrentColour(0.3f, 0.6f, 0.9f, 0.17f);
	gfx->fillRect(0.5 - 0.5 * rectSize * props.phaseOffsetLevel, 0, 0.5 + 0.5 * rectSize * props.phaseOffsetLevel, 1.f);
}


bool DeformerPanel::isEffectEnabled() const
{
	return true;
}


void DeformerPanel::buttonClicked(Button *button)
{
	if(button == &panelControls->addRemover.add ||
	   button == &panelControls->addRemover.remove)
	{
		bool forceUpdate = false;
		bool globalUpdate = false;

		{
			ScopedLock lock(getObj(SynthAudioSource).getLock());

			if(button == &panelControls->addRemover.add)
			{
				addNewLayer(false);
			}
			else if(button == &panelControls->addRemover.remove)
			{
				int index = meshLib->getGroup(LayerGroups::GroupDeformer).current;

				{
					ScopedLock lock(getObj(SynthAudioSource).getLock());
					if(index > 0)
						guideTables.erase(guideTables.begin() + index);

					setGuideBuffers();

					meshLib->removeLayerKeepingOne(LayerGroups::GroupDeformer, index);
					globalUpdate |= meshLib->layerRemoved(LayerGroups::GroupDeformer, index) || index == 0;
					meshLib->layerChanged(LayerGroups::GroupDeformer, index);
				}

				repaint();
				forceUpdate = index == 0;
			}

			rasterizeAllTables();
		}

		getObj(VertexPropertiesPanel).updateComboBoxes();
		panelControls->layerSelector->refresh(forceUpdate);

		if(globalUpdate)
			getObj(Updater).update(UpdateSources::SourceWaveform3D);
	}
}


void DeformerPanel::addNewLayer(bool update)
{
	meshLib->addLayer(LayerGroups::GroupDeformer);

	// incremented by the above
	int currentLayer = meshLib->getGroup(LayerGroups::GroupDeformer).current;

	{
		enterClientLock();
		guideTables.push_back(GuideProps(this, currentLayer, 0, 0, 0, random.nextInt(tableSize)));
		exitClientLock();
	}

	setGuideBuffers();

	if(update)
	{
		rasterizeAllTables();
		getObj(VertexPropertiesPanel).updateComboBoxes();
		panelControls->layerSelector->refresh();

		layerChanged();
	}
}


void DeformerPanel::panelResized()
{
	Panel::panelResized();

//	Rectangle<int> bounds = comp->getLocalBounds();
//	nameTexA->rect = bounds.removeFromRight(nameImage.getWidth()).removeFromTop(nameImage.getHeight()).toFloat();


//	controls.resized();
}


void DeformerPanel::sliderDragEnded(Slider *slider)
{
	// this usually gets updated in updateDspSync, via postUpdateMessage,
	// but when we're no updating in realtime there are none of these calls
	if(! getSetting(UpdateGfxRealtime))
		rasterizeTable();

	triggerRestoreUpdate();
}


void DeformerPanel::sliderDragStarted(Slider *slider)
{
	if(getSetting(UpdateGfxRealtime))
		triggerReduceUpdate();
}


void DeformerPanel::sliderValueChanged(Slider *slider)
{
	progressMark

	int currentLayer = meshLib->getGroup(LayerGroups::GroupDeformer).current;

	GuideProps& props = guideTables[currentLayer];

	if(slider == &noise)
	{
		props.noiseLevel = powf(noise.getValue(), 2);
	}
	else if(slider == &vertOffset)
	{
		props.vertOffsetLevel = powf(vertOffset.getValue(), 2);
	}
	else if(slider == &phaseOffset)
	{
		props.phaseOffsetLevel = powf(phaseOffset.getValue(), 2);
	}

	if(getSetting(UpdateGfxRealtime))
		postUpdateMessage();
}


void DeformerPanel::layerChanged()
{
	progressMark

	rasterizer->cleanUp();
	rasterizer->setMesh(meshLib->getCurrentMesh(layerType));
	clearSelectedAndCurrent();

	meshLib->layerChanged(LayerGroups::GroupDeformer, -1);
	updateKnobsImplicit();
	getRasterizer()->performUpdate(UpdateType::Update);
	performUpdate(UpdateType::Update);
	// this should only trigger updates to self if before envelope viewstage
//	postUpdateMessage();
}


void DeformerPanel::updateKnobsImplicit()
{
	int currentLayer = meshLib->getGroup(layerType).current;

	if(! isPositiveAndBelow(currentLayer, (int) guideTables.size()))
		return;

	GuideProps& props = guideTables[currentLayer];
	float noiseValue = sqrtf(props.noiseLevel);
	noise.setValue(noiseValue, dontSendNotification);

	float offsetValue = sqrtf(props.vertOffsetLevel);
	vertOffset.setValue(offsetValue, dontSendNotification);

	float phaseValue = sqrtf(props.phaseOffsetLevel);
	phaseOffset.setValue(phaseValue, dontSendNotification);
}


void DeformerPanel::setCurrentMesh(Mesh* mesh)
{
	meshLib->setCurrentMesh(layerType, mesh);

	clearSelectedAndCurrent();
	setMeshAndUpdate(mesh);
	rasterizeTable();

	triggerRefreshUpdate();
}


void DeformerPanel::previewMesh(Mesh* mesh)
{
	ScopedLock sl(renderLock);

	setMeshAndUpdate(mesh);
}


void DeformerPanel::previewMeshEnded(Mesh* oldMesh)
{
	ScopedLock sl(renderLock);

	setMeshAndUpdate(oldMesh);
}


void DeformerPanel::setMeshAndUpdate(Mesh *mesh)
{
	mesh->updateToVersion(repo);

	rasterizer->cleanUp();
	rasterizer->setMesh(mesh);
	rasterizer->performUpdate(UpdateType::Update);
	repaint();
}


void DeformerPanel::enterClientLock()
{
	getObj(SynthAudioSource).getLock().enter();
	renderLock.enter();
}


void DeformerPanel::exitClientLock()
{
	renderLock.exit();
	getObj(SynthAudioSource).getLock().exit();
}


Mesh* DeformerPanel::getCurrentMesh()
{
	return Interactor::getMesh();
}


bool DeformerPanel::setGuideBuffers()
{
	// XXX causes deadlock
	enterClientLock();

	bool changed = dynMemory.ensureSize(guideTables.size() * tableSize);

	for(int i = 0; i < (int) guideTables.size(); ++i)
		guideTables[i].table = dynMemory.place(tableSize);

	exitClientLock();

	return changed;
}


bool DeformerPanel::readXML(const XmlElement* element)
{
	XmlElement* deformerElem = element->getChildByName("DeformerProps");

	guideTables.clear();
	int index = 0;

  #ifndef BEAT_EDITION
	int numMeshes = meshLib->getGroup(layerType).size();

	if(deformerElem != nullptr)
	{
		forEachXmlChildElementWithTagName(*deformerElem, propsElem, "Properties")
		{
			if(guideTables.size() >= numMeshes)
				break;

			float noiseLevel 	= propsElem->getDoubleAttribute ("noiseLevel",  0);
			float offsetLevel 	= propsElem->getDoubleAttribute ("offsetLevel", 0);
			float phaseLevel 	= propsElem->getDoubleAttribute ("phaseLevel",  0);
			int seed			= propsElem->getIntAttribute	("noiseSeed",  -1);

			if(seed == -1)
				seed = random.nextInt(tableSize);

			guideTables.push_back(GuideProps(this, index++, noiseLevel, offsetLevel, phaseLevel, seed));
		}
	}


	while((int) guideTables.size() < numMeshes)
	{
		guideTables.push_back(GuideProps(this, index++, 0, 0, 0, random.nextInt(tableSize)));
	}

	panelControls->layerSelector->reset();

	updateKnobsImplicit();
	setGuideBuffers();
	rasterizeAllTables();

  #endif
	return true;
}


int DeformerPanel::getTableDensity(int index)
{
	MeshLibrary::Layer& layer = meshLib->getLayer(layerType, index);

	if(layer.mesh == nullptr)
		return 0;

	return layer.mesh->getNumVerts();
}


void DeformerPanel::writeXML(XmlElement* element) const
{
  #ifndef DEMO_VERSION
	XmlElement* deformerElem = new XmlElement("DeformerProps");

	for(int i = 0; i < (int) guideTables.size(); ++i)
	{
		XmlElement* propsElem 	= new XmlElement("Properties");
		const GuideProps& props = guideTables[i];

		propsElem->setAttribute("noiseLevel", 	props.noiseLevel);
		propsElem->setAttribute("offsetLevel", 	props.vertOffsetLevel);
		propsElem->setAttribute("phaseLevel", 	props.phaseOffsetLevel);
		propsElem->setAttribute("noiseSeed", 	props.seed);

		deformerElem->addChildElement(propsElem);
	}

	element->addChildElement(deformerElem);
  #endif
}


void DeformerPanel::sampleDownAddNoise(int index,
									   Buffer<float> dest,
									   const IDeformer::NoiseContext& context)
{
	int length = dest.size();

	GuideProps& props 	= guideTables[index];
	Buffer<Ipp32f> table = props.table;

	dest.downsampleFrom(table);

	length = dest.size();

	if(props.phaseOffsetLevel > 0)
	{
		int phaseOffset = (context.phaseOffset & tableModulo - tableSize / 2) * props.phaseOffsetLevel;

		dest.withPhase(phaseOffset % length, phaseMoveBuffer.withSize(length));
	}

	if(props.noiseLevel > 0.f)
	{
		int noiseOffset = (props.seed + context.noiseSeed) & tableModulo;
		int elemsToCopy = jmin(length, noiseArray.size() - noiseOffset);

		dest.addProduct(noiseArray.section(noiseOffset, elemsToCopy), props.noiseLevel);

		if(length - elemsToCopy > 0)
			dest.offset(elemsToCopy).addProduct(noiseArray, props.noiseLevel);
	}

	if(props.vertOffsetLevel > 0.f)
	{
		int offset = (props.seed + context.vertOffset) & tableModulo;
		dest.add(props.vertOffsetLevel * noiseArray[offset]);
	}
}


void DeformerPanel::reset()
{
//	panelControls->resetLayerBox();

	panelControls->layerSelector->reset();
}


void DeformerPanel::doubleMesh()
{
	getCurrentMesh()->twin(getConstant(DeformerPadding), getConstant(DeformerPadding));
	postUpdateMessage();
}


Component* DeformerPanel::getComponent(int which)
{
	switch(which)
	{
		case CycleTour::TargDfrmNoise: 		return &noise;
		case CycleTour::TargDfrmOffset: 	return &vertOffset;
		case CycleTour::TargDfrmPhase: 		return &phaseOffset;
		case CycleTour::TargDfrmMeshSlct: 	return meshSelector;
		case CycleTour::TargDfrmLayerAdd:	return &panelControls->addRemover;
		case CycleTour::TargDfrmLayerSlct:	return panelControls->layerSelector;
	}

	return nullptr;
}


void DeformerPanel::triggerButton(int id)
{
	switch(id)
	{
		case CycleTour::IdBttnAdd:		buttonClicked(&panelControls->addRemover.add); 		break;
		case CycleTour::IdBttnRemove:	buttonClicked(&panelControls->addRemover.remove); 	break;
	}
}


void DeformerPanel::showCoordinates()
{
	float invSize 	= 1.f / float(1.f - 2.f * getConstant(DeformerPadding));
	float xformX 	= (state.currentMouse.x - getConstant(DeformerPadding)) * invSize;

	String message 	= String(xformX, 2) + ", " + String(state.currentMouse.y, 2);
	showMsg(message);
}

#include <algorithm>
#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Curve/CollisionDetector.h>
#include <Inter/Interactor.h>
#include <Inter/Interactor3D.h>
#include <Inter/UndoableActions.h>
#include <iterator>
#include <UI/IConsole.h>
#include <UI/MiscGraphics.h>
#include <UI/Panels/Panel.h>
#include <UI/Panels/Panel3D.h>
#include <UI/Widgets/Knob.h>

#include "MainPanel.h"
#include "VertexPropertiesPanel.h"

#include "Morphing/MorphPanel.h"
#include "../VertexPanels/DeformerPanel.h"
#include "../Widgets/HSlider.h"

#include "../../App/CycleTour.h"
#include "../../App/Dialogs.h"
#include "../../Inter/EnvelopeInter2D.h"
#include "../../Inter/WaveformInter3D.h"
#include "../../UI/Effects/IrModellerUI.h"
#include "../../UI/Effects/WaveshaperUI.h"

using std::set;

VertexPropertiesPanel::VertexPropertiesPanel(SingletonRepo* repo) : 
		SingletonAccessor(repo, "VertexPropertiesPanel")
	, 	currentInteractor(0)
	,	gainListener	(this)
	,	ampVsPhaseStr	("component curve")
	,	title			(repo, "VERTEX PARAMS")
{
	title.setColorHSV(0.f, 0.25f);
}

VertexPropertiesPanel::~VertexPropertiesPanel() {
	for (int i = 0; i < (int) allProperties.size(); ++i) {
		VertexProperties* p = allProperties[i];
		delete p->dfrmChanBox;
		delete p->gain;
		delete p->messager;
		delete p->slider;
	}
}

void VertexPropertiesPanel::init() {
	properties.push_back(VertexProperties(this, "time", 	"Time", 				Vertex::Time));
	properties.push_back(VertexProperties(this, "phase", 	"Phase", 				Vertex::Phase));
	properties.push_back(VertexProperties(this, "amp", 		"Amplitude", 			Vertex::Amp));
	properties.push_back(VertexProperties(this, "red", 		"Red-range value", 		Vertex::Red));
	properties.push_back(VertexProperties(this, "blue", 	"Blue-range Value", 	Vertex::Blue));
	properties.push_back(VertexProperties(this, "curve", 	"Curve Sharpness", 		Vertex::Curve));
	ampVsPhaseProperties = std::make_unique<VertexProperties>(this, String(), String(), Vertex::Time);

	gainProperties.push_back(ampVsPhaseProperties.get());

	for(int i = 1; i < (int) properties.size(); ++i)
		gainProperties.push_back(&properties[i]);

	allProperties.push_back(ampVsPhaseProperties.get());

	for(int i = 0; i < (int) properties.size(); ++i)
		allProperties.push_back(&properties[i]);

	goldfish = getObj(MiscGraphics).getVerdana12();
	silkscreen = getObj(MiscGraphics).getSilkscreen();

	addAndMakeVisible(&sliderArea);
	addAndMakeVisible(&boxArea);
	addAndMakeVisible(&gainArea);
}

void VertexPropertiesPanel::paint(Graphics& g) {
	g.fillAll(Colour::greyLevel(0.16));

	Slider& timeSlider 	= *properties[Vertex::Time].slider;
	Slider& curveSlider	= *properties[Vertex::Curve].slider;

	String titleText	= "vertex params";
	Font& font 			= *getObj(MiscGraphics).getSilkscreen();
	int titleTextX		= timeSlider.getBounds().getCentreX() - font.getStringWidth(titleText) / 2;
	getObj(MiscGraphics).drawShadowedText(g, titleText, 	titleTextX, timeSlider.getY() - 5, font);

	Knob& gainKnob		= *properties[Vertex::Red].gain;
	ComboBox& dfrmBox 	= *properties[Vertex::Red].dfrmChanBox;

	Rectangle<float> titleBounds(title.getBounds().expanded(30, 30).translated(0, 20).toFloat());

	g.setColour(Colour::greyLevel(0.55f));
	g.setFont(font);

	int avpWidth = font.getStringWidth(ampVsPhaseStr);

	g.drawSingleLineText(ampVsPhaseStr,
	                     curveSlider.getX() + (curveSlider.getWidth() - avpWidth - 5),
	                     curveSlider.getY() + curveSlider.getHeight() * 3 / 2 + 5);

	bool isSmall 		= (dfrmBox.getWidth() < 20);
	String gainText 	= isSmall ? "gn" 	: "gain";
	String dfrmTextA 	= isSmall ? "DFM" 	: "DFRM";
	String dfrmTextB 	= isSmall ? "CH" 	: "CHAN";

	int guideTextX 		= timeSlider.getRight() + 1 + (dfrmBox.getWidth() - font.getStringWidth(dfrmTextA)) / 2;
	int gainTextX 		= dfrmBox.getRight() + 1 + (gainKnob.getWidth() - font.getStringWidth(gainText)) / 2;

	float alpha = 0.65f;
	getObj(MiscGraphics).drawShadowedText(g, dfrmTextA, guideTextX + 14, timeSlider.getY() - 7, font, alpha);
	getObj(MiscGraphics).drawShadowedText(g, dfrmTextB, guideTextX, timeSlider.getY() + 4, 	font, alpha);
	getObj(MiscGraphics).drawShadowedText(g, gainText, 	gainTextX, 	timeSlider.getY() + 4, 	font, alpha);

	Rectangle tSlider(properties[Vertex::Time].slider->getBounds());

	g.setColour(Colours::black);
	g.drawRect(ampVsPhaseProperties->dfrmChanBox->getBounds()
	           .withX(tSlider.getX()).withWidth(tSlider.getWidth()));
}

void VertexPropertiesPanel::resized() {
	int knobWidth = (getHeight() - 32) / 7;
	int rightSize = knobWidth * 2 + 4;

	vector<VertexProperties*> displayOrder;
	displayOrder.push_back(&properties[Vertex::Time] );
	displayOrder.push_back(&properties[Vertex::Red]	 );
	displayOrder.push_back(&properties[Vertex::Blue] );
	displayOrder.push_back(&properties[Vertex::Phase]);
	displayOrder.push_back(&properties[Vertex::Amp]	 );
	displayOrder.push_back(&properties[Vertex::Curve]);

	Rectangle<int> bounds = getLocalBounds().reduced(0, 4);
	bounds.removeFromLeft(4);
	bounds.removeFromTop(5);

	Rectangle<int> labelBounds = bounds.removeFromRight(rightSize);

	bounds.removeFromRight(4);

	for (int i = 0; i < (int) displayOrder.size(); ++i) {
		VertexProperties* props = displayOrder[i];
		props->slider->setBounds(bounds.removeFromTop(knobWidth));

		Rectangle<int> deformBounds = labelBounds.removeFromTop(knobWidth);

		if(props->id != Vertex::Time)
		{
			props->dfrmChanBox->setBounds(deformBounds.removeFromLeft(jmax(24, knobWidth)));

			deformBounds.removeFromLeft(2);

			int size = jmin(deformBounds.getWidth(), deformBounds.getHeight());
			props->gain->setBounds(Rectangle(deformBounds.getX(), deformBounds.getY(), size, size));
		}

		bounds.removeFromTop(2);
		labelBounds.removeFromTop(2);
	}

	bounds.removeFromTop(knobWidth + 2);

	sliderArea.toBack();
	sliderArea.setBounds(Rectangle(properties[Vertex::Time].slider->getPosition(),
	                                    properties[Vertex::Curve].slider->getBounds().getBottomRight()));

	Rectangle<int> deformBounds = labelBounds.removeFromTop(knobWidth);
	ampVsPhaseProperties->dfrmChanBox->setBounds(deformBounds.removeFromLeft(jmax(24, knobWidth)));

	deformBounds.removeFromLeft(2);
	int size = jmin(deformBounds.getWidth(), deformBounds.getHeight());
	ampVsPhaseProperties->gain->setBounds(Rectangle(deformBounds.getX(), deformBounds.getY(), size, size));


	gainArea.toBack();
	gainArea.setBounds(Rectangle(properties[Vertex::Red].gain->getPosition(),
	                                  ampVsPhaseProperties->gain->getBounds().getBottomRight()));

	boxArea.toBack();
	boxArea.setBounds(Rectangle(properties[Vertex::Red].dfrmChanBox->getPosition(),
	                                 ampVsPhaseProperties->dfrmChanBox->getBounds().getBottomRight()));

	labelBounds.removeFromTop(2);
}

void VertexPropertiesPanel::mouseEnter(const MouseEvent& e) {
	Console* console = dynamic_cast<Console*>(&getObj(IConsole));
	console->updateAll({}, "Parameters of selected vertices. Deformers warp path of parent lines",
					   MouseUsage(false, false, false, false));
}

void VertexPropertiesPanel::mouseUp(const MouseEvent& e) {
}

void VertexPropertiesPanel::sliderValueChanged(Slider* slider) {
	if(! currentInteractor)
		return;

	Interactor::VertexProps& itrProps = currentInteractor->vertexProps;

	for(int i = 0; i < numSliders; ++i) {
		if (slider == properties[i].slider) {
			if (!itrProps.sliderApplicable[i]) {
				showMsg("Dimension not applicable in this context");
				return;
			}
		}
	}

	VertexProperties* props = nullptr;

	for (int i = 0; i < (int) properties.size(); ++i) {
		if (properties[i].slider == slider) {
			props = &properties[i];
			break;
		}
	}

	int id = props->id;

	const vector<VertexFrame>& movableVerts = currentInteractor->getSelectedMovingVerts();

	ScopedAlloc<Ipp32f> diffs(movableVerts.size());

	jassert(movableVerts.size() < 10000);

	for(int i = 0; i < movableVerts.size(); ++i) {
		const VertexFrame& frame = movableVerts[i];

		float& value = frame.vert->values[id];
		float oldVal = value;

		value += (slider->getValue() - props->previousValue);
		NumberUtils::constrain<float>(value, currentInteractor->vertexLimits[id]);

		diffs[i] = value - oldVal;
	}

	currentInteractor->getCollisionDetector().setCurrentSelection(currentInteractor->getMesh(), movableVerts);

	if(currentInteractor->getCollisionDetector().validate()) {
		props->previousValue = slider->getValue();

		currentInteractor->resizeFinalBoxSelection(true);

		if (slider == properties[Vertex::Time].slider ||
		    slider == properties[Vertex::Red].slider ||
		    slider == properties[Vertex::Blue].slider) {
			set<VertCube*> lines;

			vector<Vertex*>& selected = currentInteractor->getSelected();
			for (vector<Vertex*>::iterator it = selected.begin(); it != selected.end(); ++it) {
				Vertex* vert = *it;

				if (it == selected.begin()) {
					VertCube* line = currentInteractor->getClosestLine(vert);
					if(line != nullptr)
						lines.insert(line);
				}
			}

			refreshCube(lines);
		}

		if(getSetting(UpdateGfxRealtime)) {
			currentInteractor->triggerRefreshUpdate();
		}
	} else {
		for (int i = 0; i < movableVerts.size(); ++i)
			movableVerts[i].vert->values[id] -= diffs[i];

		props->setValueToCurrent(false);
		showMsg("Could not move without overlapping lines");
	}

	if (itrProps.isEnvelope) {
		getObj(EnvelopeInter2D).syncEnvPointsImplicit();
	}
}

void VertexPropertiesPanel::setSelectedAndCaller(Interactor* interactor)
{
	currentInteractor = interactor;

	updateSliderProperties();
	updateSliderValues(true);
}

void VertexPropertiesPanel::updateComboBoxes() {
	MeshLibrary& meshLib = getObj(MeshLibrary);

	int numDeformLayers = meshLib.getGroup(LayerGroups::GroupDeformer).size();

	vector<ComboBox*> boxes;

	for (int i = 1; i < numSliders; ++i) {
		boxes.push_back(properties[i].dfrmChanBox);
	}

	boxes.push_back(ampVsPhaseProperties->dfrmChanBox);

	for (int i = 0; i < (int) boxes.size(); ++i) {
		ComboBox* box = boxes[i];
		box->clear(dontSendNotification);
		box->addItem(String(L"\u2013"), NullDfrmId);

		for (int j = 0; j < numDeformLayers; ++j)
			box->addItem(String(j + 1), j + 2);
	}

	refreshValueBoxesFromSelected();
}

void VertexPropertiesPanel::updateSliderValues(bool ignoreChangeMessage)
{
	if(! currentInteractor) {
		return;
	}

	vector<Vertex*>& selected = currentInteractor->getSelected();

	if (selected.empty()) {
		for (int i = 0; i < numSliders; ++i) {
			VertexProperties& props = properties[i];

			props.slider->setValue(0., ignoreChangeMessage ? dontSendNotification : sendNotificationAsync);

			if (props.dfrmChanBox != nullptr) {
				props.dfrmChanBox->setSelectedId(NullDfrmId, dontSendNotification);
			}

			if (props.gain) {
				props.gain->setValue(0.5, ignoreChangeMessage ? dontSendNotification : sendNotificationAsync);
			}

			if (i == Vertex::Amp) {
				ampVsPhaseProperties->dfrmChanBox->setSelectedId(NullDfrmId, dontSendNotification);
				ampVsPhaseProperties->gain->setValue(
					0.5, ignoreChangeMessage ? dontSendNotification : sendNotificationAsync);
			}
		}

		return;
	}

	refreshValueBoxesFromSelected();

	float invSlctSize = 1 / float(selected.size());

	for(auto& props : properties) {
		props.previousValue = 0;
	}

	for(auto& props : gainProperties) {
		props->previousGain = 0;
	}

	bool wrapsPhase = currentInteractor->getRasterizer()->wrapsVertices();

	set<VertCube*> lines;

	bool isFirst = true;
	for(auto& vert : selected) {
		if (isFirst) {
			isFirst = false;
			VertCube* line = currentInteractor->getClosestLine(vert);
			if(line != nullptr) {
				lines.insert(line);
			}
		}

		// set previous values?
		for (int i = 0; i < (int) properties.size(); ++i) {
			int idx = i;

			if (idx == Vertex::Phase && wrapsPhase) {
				float x = vert->values[idx];
				if (x > 1) {
					x -= 1;
				}

				properties[idx].previousValue += x * invSlctSize;
			}
			else
			{
				properties[idx].previousValue += vert->values[idx] * invSlctSize;
			}
		}
	}

	refreshCube(lines);

	float invLinesSize = 1 / float(lines.size());

	for(auto& cube : lines) {
		for(auto& props : gainProperties) {
			props->previousGain += cube->dfrmGainAt(props->id) * invLinesSize;
		}
	}

	for(auto& props : allProperties) {
		props->setValueToCurrent(! ignoreChangeMessage);
		props->setGainValueToCurrent(! ignoreChangeMessage);
	}

	properties[CurveSldr].slider->name = "curve";

	if (lines.size() == 1) {
		if (ampVsPhaseProperties->dfrmChanBox->getSelectedId() != NullDfrmId) {
			properties[CurveSldr].slider->name = "Dfrm gain";
		}
	}
}

void VertexPropertiesPanel::refreshCube(set<VertCube*>& lines)
{
	vector<Vertex*> selected = currentInteractor->getSelected();

	int scratchChannel = CommonEnums::Null;
	if(lines.size() == 1 && selected.size() == 1) {
		Interactor3D* itr3 = dynamic_cast<Interactor3D*>(currentInteractor);

		if (itr3 == nullptr) {
			Interactor* opposite = currentInteractor->getOppositeInteractor();
			itr3 = dynamic_cast<Interactor3D*>(opposite);
		}

		if (itr3 != nullptr) {
			Panel3D* panel3 = static_cast<Panel3D*>(itr3->panel.get());
			scratchChannel = panel3->getLayerScratchChannel();
		}

		Interactor::VertexProps& itrProps = currentInteractor->vertexProps;

		Vertex* selectedVert = *selected.begin();
		VertCube* selectedLine = *lines.begin();

		getObj(MorphPanel).setSelectedCube(selectedVert, selectedLine,
		                                    scratchChannel, itrProps.isEnvelope);
	} else {
		getObj(MorphPanel).setSelectedCube(nullptr, nullptr, scratchChannel, false);
	}
}

void VertexPropertiesPanel::sliderDragEnded(Slider* slider) {
	if (!currentInteractor) {
		return;
	}

	/*
	if(slider == properties[Vertex::Time].slider ||
			slider == properties[Vertex::Key].slider ||
			slider == properties[Vertex::Mod].slider)
	{
		set<VertCube*> lines;

		vector<Vertex*>& selected = currentInteractor->getSelected();
		for(vector<Vertex*>::iterator it = selected.begin(); it != selected.end(); ++it)
		{
			Vertex* vert = *it;

			if(vert->ownerA != nullptr)
				lines.insert(vert->ownerA);
		}

		refreshCube(lines);
	}
	 */

	currentInteractor->triggerRestoreUpdate();
}

void VertexPropertiesPanel::sliderDragStarted(Slider* slider) {
	if (!currentInteractor) {
		return;
	}

	if (getSetting(UpdateGfxRealtime)) {
		currentInteractor->triggerReduceUpdate();
	}
}

void VertexPropertiesPanel::drawJustifiedText(Graphics& g, String text, Component* component, int y) {
	int width 	 = component->getWidth();
	int strWidth = silkscreen->getStringWidth(text);
	int x 		 = component->getX() + (width - strWidth) / 2;

	g.setColour(Colour(15, 15, 15));
	g.drawSingleLineText(text, x + 1, y + 1);
	g.setColour(Colours::darkgrey);
	g.drawSingleLineText(text, x, y);
}


void VertexPropertiesPanel::drawSidewaysTextAndLines(Graphics& g, String text, Component* top,
		Component* bottom)
{
	/*
	int height 		= (bottom->getBottom() - top->getY());
	int strWidth 	= silkscreen->getStringWidth(text);
	int y 			= top->getY() + (height - strWidth) / 2;
	int x 			= 4 * getWidth() / 5;

	AffineTransform transform;
	transform = transform.rotated(M_PI_2);
	transform = transform.translated(x, y);

	g.setColour(Colour(15, 15, 15));
	g.drawTextAsPath(text, transform);
	g.setColour(Colour(90, 90, 90));
	g.drawTextAsPath(text, transform.translated(1, -1));

	g.setColour(Colour(60, 60, 60));
	x += 3;

	if(y - 5 > top->getY() + 5)
	{
		g.drawLine(x + 0.5f, top->getY() + 5, x + 0.5f, y - 5);
		g.drawLine(x + 0.5f, y + strWidth + 5, x + 0.5f, bottom->getBottom() - 5);
	}
	*/
}

void VertexPropertiesPanel::buttonClicked(Button* button) {
}

void VertexPropertiesPanel::comboBoxChanged(ComboBox* box) {
	if (!currentInteractor) {
		box->setSelectedId(NullDfrmId, dontSendNotification);

		return;
	}

	int dim = -1;
	for (int i = 0; i < numSliders; ++i) {
		if (box == properties[i].dfrmChanBox)
			dim = i;
	}

	if (dim < 0) {
		if (!currentInteractor->vertexProps.ampVsPhaseApplicable) {
			showMsg("This dimension is not applicable in this context");
			return;
		}

		dim = Vertex::Time;
		jassert(box == ampVsPhaseProperties->dfrmChanBox);
	} else {
		if (!currentInteractor->vertexProps.sliderApplicable[dim]) {
			showMsg("This dimension is not applicable in this context");
			return;
		}
	}

	vector<Vertex*>& selected = currentInteractor->getSelected();

	if (selected.empty()) {
		box->setSelectedId(NullDfrmId, dontSendNotification);

		showMsg("Select a vertex first!");
		return;
	}

	int id = box->getSelectedId();
	int guideIndex = -1;

	if (id == NullDfrmId) {
		// leave as is
	} else {
		guideIndex = id - NullDfrmId - 1;
	}

	std::cout << "set guide dim to " << dim << "\n";

	vector<VertCube*> affectedLines;
	vector<int> previousMappings;

	for(auto& vert : selected) {
		for(auto& cube : vert->owners) {
			affectedLines.push_back(cube);
			previousMappings.push_back(cube->deformerAt(dim));
		}
	}

	getObj(EditWatcher).addAction(
			new DeformerAssignment(currentInteractor, currentInteractor->getMesh(),
			                       affectedLines, previousMappings, guideIndex, dim), true);

	triggerAsyncUpdate();
}

void VertexPropertiesPanel::gainChanged(Slider* slider, int changeType)
{
	if(! currentInteractor)
		return;

	VertexProperties* props = nullptr;
	for(auto& gProps : gainProperties) {
		if(gProps->gain == slider) {
			props = gProps;
			break;
		}
	}

	if(props == nullptr) {
		return;
	}

	int id 				= props->id;
	bool deformEnabled 	= currentInteractor->vertexProps.deformApplicable[id];
	deformEnabled 	   &= props->dfrmChanBox->getSelectedId() != NullDfrmId;

	if(deformEnabled) {
		if (changeType == ValueChanged) {
			vector<Vertex*>& selected = currentInteractor->getSelected();

			for(auto& vert : selected) {
				VertCube* cube = vert->owners.getFirst();

				if (cube != nullptr) {
					float& unitValue = cube->dfrmGainAt(id);
					unitValue = jlimit<float>(0.f, 1.f, unitValue + (slider->getValue() - props->previousGain));
				}
			}
		}
	}

	props->previousGain = slider->getValue();

	if(deformEnabled) {
		switch (changeType) {
			case ValueChanged: 	currentInteractor->postUpdateMessage(); 	break;
			case DragStarted:	currentInteractor->triggerReduceUpdate();	break;
			case DragEnded: 	currentInteractor->triggerRestoreUpdate(); 	break;
			default:
				break;
		}
	}
}

Component* VertexPropertiesPanel::getComponent(int which) {
	switch (which) {
		case CycleTour::TargSliderArea: return &sliderArea;
		case CycleTour::TargTimeSlider: return properties[Vertex::Time].slider;
		case CycleTour::TargPhsSlider: 	return properties[Vertex::Phase].slider;
		case CycleTour::TargAmpSlider: 	return properties[Vertex::Amp].slider;
		case CycleTour::TargKeySlider: 	return properties[Vertex::Red].slider;
		case CycleTour::TargMorphSlider: return properties[Vertex::Blue].slider;
		case CycleTour::TargCrvSlider: 	return properties[Vertex::Curve].slider;

		case CycleTour::TargBoxArea: 	return &boxArea;
		case CycleTour::TargPhsBox: 	return properties[Vertex::Phase].dfrmChanBox;
		case CycleTour::TargAmpBox: 	return properties[Vertex::Amp].dfrmChanBox;
		case CycleTour::TargKeyBox: 	return properties[Vertex::Red].dfrmChanBox;
		case CycleTour::TargModBox: 	return properties[Vertex::Blue].dfrmChanBox;
		case CycleTour::TargCrvBox: 	return properties[Vertex::Curve].dfrmChanBox;
		case CycleTour::TargAvpBox: 	return ampVsPhaseProperties->dfrmChanBox;

		case CycleTour::TargGainArea: 	return &gainArea;
		case CycleTour::TargPhsGain: 	return properties[Vertex::Phase].gain;
		case CycleTour::TargAmpGain: 	return properties[Vertex::Amp].gain;
		case CycleTour::TargKeyGain: 	return properties[Vertex::Red].gain;
		case CycleTour::TargModGain: 	return properties[Vertex::Blue].gain;
		case CycleTour::TargCrvGain: 	return properties[Vertex::Curve].gain;
		case CycleTour::TargAvpGain: 	return ampVsPhaseProperties->gain;
		default: break;
	}

	return nullptr;
}

void VertexPropertiesPanel::refreshValueBoxesFromSelected() {
	if(currentInteractor != nullptr && currentInteractor->dims.numHidden() > 0) {
		vector<Vertex*>& selected = currentInteractor->getSelected();

		bool common[numSliders];
		int guideLayerIdx[numSliders];

		for (int i = 0; i < numSliders; ++i) {
			common[i] = true;
			guideLayerIdx[i] = -1;
		}

		for(int vertIdx = 0; vertIdx < (int) selected.size(); ++vertIdx) {
			Vertex* vert = selected[vertIdx];
			VertCube* a = vert->owners.getFirst();

			if(a == nullptr) {
				continue;
			}

			for (int j = 0; j < numSliders; ++j) {
				if(vertIdx == 0) {
					guideLayerIdx[j] = a->deformerAt(j);
				} else {
					common[j] &= (guideLayerIdx[j] == a->deformerAt(j));
				}
			}
		}

		String dash(L"\u2013");

		for (int i = 0; i < numSliders; ++i) {
			common[i] &= guideLayerIdx[i] >= 0;

			VertexProperties* props = nullptr;
			if (i == 0) {
				props = ampVsPhaseProperties.get();
			} else {
				props = &properties[i];
			}

			props->dfrmChanBox->setSelectedId(guideLayerIdx[i] < 0 ? NullDfrmId : guideLayerIdx[i] + 1 + NullDfrmId,
			                                  dontSendNotification);
		}
	}
}

VertexPropertiesPanel::VertexProperties::~VertexProperties() {
	gain 		= nullptr;
	slider 		= nullptr;
	messager	= nullptr;
	dfrmChanBox	= nullptr;
}

VertexPropertiesPanel::VertexProperties::VertexProperties(
		VertexPropertiesPanel* panel
	,	const String& name
	,	const String& text
	,	int id) :
			previousValue(0)
		,	previousGain(0.5)
		,	id			(id)
		,	gain		(nullptr)
		,	slider		(nullptr)
		,	messager	(nullptr)
		,	dfrmChanBox	(nullptr)
{
	StringArray names;

	names.add("Time");
	names.add("Phase");
	names.add("Amp");
	names.add("Key");
	names.add("Mod");
	names.add("Curve");

	if (panel->currentInteractor != nullptr) {
		Interactor::VertexProps& itrProps = panel->currentInteractor->vertexProps;
		names = itrProps.dimensionNames;
	}

	SingletonRepo* repo  = panel->repo;
	EditWatcher& watcher = getObj(EditWatcher);

	if (name.isNotEmpty()) {
		panel->addAndMakeVisible(slider = new HSlider(repo, name, text, true));

		slider->addListener(panel);
		slider->setRange(0, 1);
	}

	if (name != "time") {
		panel->addAndMakeVisible(dfrmChanBox = new ComboBox());

		dfrmChanBox->addListener(panel);
		dfrmChanBox->setWantsKeyboardFocus(false);
		dfrmChanBox->setMouseClickGrabsKeyboardFocus(false);
		dfrmChanBox->setTextWhenNothingSelected(String(L"\u2013"));
		dfrmChanBox->setColour(ComboBox::outlineColourId, Colours::black);

		int srcId, destId;
		panel->getSourceDestDimensionIds(id, srcId, destId);

		messager = new MouseOverMessager(repo, "Set deform channel for " + names[srcId] + " versus " + names[destId], dfrmChanBox);

		panel->addAndMakeVisible(gain = new Knob(panel->repo));

		using namespace Ops;
		StringFunction decibel30 = StringFunction(1).chain(Mul, 2.f).chain(Add, -1.f).chain(Mul, 30.f);

		gain->setRange(0, 1);
		gain->addListener(&panel->gainListener);
		gain->setHint("Deformer gain");
		gain->setColour(Colour::greyLevel(0.4f));
		gain->setStringFunctions(decibel30, decibel30.withPostString(" dB"));
		gain->setDrawValueText(false);

		if(id == Vertex::Red || id == Vertex::Blue)
		{
//			dfrmChanBox->setEnabled(false);
//			gain->setEnabled(false);
		}
	}
}

void VertexPropertiesPanel::VertexProperties::setValueToCurrent(bool sendChangeMessage)
{
	if(slider != nullptr) {
		slider->setValue(previousValue, sendChangeMessage ? sendNotificationAsync : dontSendNotification);
	}
}

void VertexPropertiesPanel::VertexProperties::setGainValueToCurrent(bool sendChangeMessage)
{
	if(gain != nullptr) {
		gain->setValue(previousGain, sendChangeMessage ? sendNotificationAsync : dontSendNotification);
	}
}

VertexPropertiesPanel::VertexProperties& VertexPropertiesPanel::VertexProperties::operator=(
		const VertexProperties& copy)
{
	previousGain 	= copy.previousGain;
	previousValue 	= copy.previousValue;

	slider 			= copy.slider;
	dfrmChanBox 	= copy.dfrmChanBox;
	gain 			= copy.gain;
	messager		= copy.messager;

	id				= copy.id;

	return *this;
}

VertexPropertiesPanel::VertexProperties::VertexProperties(const VertexProperties& copy) {
	this->operator=(copy);
}

void VertexPropertiesPanel::updateSliderProperties() {
	Interactor::VertexProps& itrProps = currentInteractor->vertexProps;
	const StringArray& names = itrProps.dimensionNames;

	jassert(names.size() == (int) properties.size());

	int i = 0;
	for(auto& props : properties) {
		if (props.slider != nullptr) {
			props.slider->name = names[i];
			props.slider->repaint();
			props.slider->setEnabled(itrProps.sliderApplicable[i]);
		}

		int id = props.id;
		int srcId, destId;

		getSourceDestDimensionIds(id, srcId, destId);

		if(props.messager != nullptr)
		{
			props.messager->message =
					itrProps.deformApplicable[i] ? "Set deform channel for " + names[srcId] + " versus " + names[destId] : {};
		}

		if(props.dfrmChanBox != nullptr)
		{
			if(! itrProps.deformApplicable[i])
				props.dfrmChanBox->setSelectedId(NullDfrmId, dontSendNotification);

			props.dfrmChanBox->setEnabled(itrProps.deformApplicable[i]);

			if(! itrProps.deformApplicable[i])
				props.gain->setValue(0.5, dontSendNotification);

			props.gain->setEnabled(itrProps.deformApplicable[i]);
		}
		++i;
	}


  #ifndef BEAT_EDITION
	if(! itrProps.ampVsPhaseApplicable)
	{
		ampVsPhaseProperties->dfrmChanBox->setSelectedId(NullDfrmId, dontSendNotification);
		ampVsPhaseProperties->gain->setValue(0.5, dontSendNotification);
		ampVsPhaseProperties->messager->message = {};
		ampVsPhaseStr = "--";
	}
	else
	{
		ampVsPhaseProperties->messager->message = "Set deform channel for component curve"; // names[Vertex::Amp] + " versus " + names[Vertex::Phase];
		ampVsPhaseStr = "component curve"; // "(" + names[Vertex::Amp] + " versus " + names[Vertex::Phase] + ")";
	}

	ampVsPhaseProperties->dfrmChanBox->setEnabled(itrProps.ampVsPhaseApplicable);
	ampVsPhaseProperties->gain->setEnabled(itrProps.ampVsPhaseApplicable);
  #endif
}


void VertexPropertiesPanel::handleAsyncUpdate()
{
	getObj(MainPanel).grabKeyboardFocus();
}


void VertexPropertiesPanel::triggerSliderChange(int dim, float value)
{
	properties[dim].slider->setValue(value);
}


void VertexPropertiesPanel::getSourceDestDimensionIds(int id, int& srcId, int& destId)
{
	// avp
	if(id == Vertex::Time)
	{
		destId = Vertex::Phase;
		srcId = Vertex::Amp;
	}
	else if(id == Vertex::Red || id == Vertex::Blue)
	{
		srcId = Vertex::Phase;
		destId = id;
	}
	else
	{
		destId = Vertex::Time;
		srcId = id;
	}
}

#include "JuceHeader.h"
#include <Definitions.h>
#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <UI/MiscGraphics.h>

#include "ModMatrixPanel.h"

#include <UI/IConsole.h>

#include "../CycleDefs.h"
#include "../CycleGraphicsUtils.h"
#include "../Panels/Morphing/MorphPanel.h"
#include "../VertexPanels/Waveform3D.h"
#include "../../App/KeyboardInputHandler.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../UI/VertexPanels/Spectrum3D.h"
#include "../../App/CycleTour.h"

ModMatrix::ModMatrix(SingletonRepo* repo, ModMatrixPanel* panel) :
		SingletonAccessor(repo, "ModMatrix")
	,	panel(panel)
	,	tableListBox(new TableListBox({}, this))
{
	addAndMakeVisible(tableListBox.get());
}

int ModMatrix::getNumRows() {
    return panel->inputs.size();
}

void ModMatrix::resized() {
    tableListBox->setBounds(getLocalBounds());
}

void ModMatrix::paintRowBackground(Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) {
}

void ModMatrix::paintCell(Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) {
}

juce::Component* ModMatrix::refreshComponentForCell(int row,
                                              int columnId,
                                              bool isRowSelected,
                                              Component* component) {
    int column = columnIdToIndex(columnId);

    if (column < 0) {
	    return nullptr;
    }

    if (component == nullptr) {
        auto* box = new ColourCheckbox(this, row, column);
        return box;
    } else {
        delete component;

        return new ColourCheckbox(this, row, column);
    }
}

void ModMatrix::cycleDimensionMapping(int row, int col) {
    panel->cycleDimensionMapping(row, col);
}

void ModMatrix::showDimensionsPopup(ColourCheckbox* checkbox) {

    int inputId, outputId;
    panel->getIds(checkbox->row, checkbox->col, inputId, outputId);
    int mappingIndex = panel->indexOfMapping(inputId, outputId);

    int oldDim = mappingIndex < 0 ? -1 : panel->mappings.getReference(mappingIndex).dim;

    bool active = oldDim != ModMatrixPanel::YellowDim;
	bool isVoiceTime = inputId == ModMatrixPanel::VoiceTime;
	bool isEnvelope = outputId >= ModMatrixPanel::VolEnvId;
	active &= !(isVoiceTime && isEnvelope);

	PopupMenu menu;
	menu.addItem(1, "None", active, oldDim == ModMatrixPanel::NullDim);
	menu.addItem(3, "Red", 	active, oldDim == ModMatrixPanel::RedDim);
	menu.addItem(4, "Blue", active, oldDim == ModMatrixPanel::BlueDim);

	// checkbox, 0, 80, 1, 24;
	auto options = PopupMenu::Options()
		.withStandardItemHeight(24)
		.withTargetComponent(checkbox)
	;

	menu.showMenuAsync(options, [this,oldDim,mappingIndex,inputId,outputId](int result) {
		int newDim = result - 2;
			if(result != 0 && oldDim != newDim) {
				panel->mappingChanged(mappingIndex, inputId, outputId, oldDim, newDim);
			}
		}
	);

}

ModMatrixPanel::ModMatrixPanel(SingletonRepo* repo) :
		SingletonAccessor(repo, "ModMatrixPanel")
	,	ParameterGroup::Worker(repo, "ModMatrixPanel")
	,	modMatrix		(repo, this)
	,	addDestBtn		("Add Dest")
	,	addInputBtn		("Add Source")
	,	closeButton		("Close")
	,	inputLabel		("", "Add source")
	,	destLabel		("", "Add dest")
	,	pendingFocusGrab(false)
	,	dfltModMapping	(ModWheel) {
	addAndMakeVisible(&modMatrix);

	TableListBox& listbox = modMatrix.getListbox();

	ScrollBar& vertScrollbar = listbox.getViewport()->getVerticalScrollBar();
	ScrollBar& horzScrollbar = listbox.getViewport()->getHorizontalScrollBar();

	listbox.setRowHeight(gridSize);
	listbox.getHeader().setVisible(false);
	listbox.setHeaderHeight(0);

	addDestBtn.getProperties().set("inactive", "");
	addInputBtn.getProperties().set("inactive", "");

	vertScrollbar.addListener(this);
	horzScrollbar.addListener(this);

	listbox.updateContent();

	addAndMakeVisible(&addDestBtn);
	addAndMakeVisible(&addInputBtn);
	addAndMakeVisible(&utilityArea);
	addAndMakeVisible(&sourceArea);
	addAndMakeVisible(&destArea);
	addAndMakeVisible(&closeButton);

	addDestBtn.addListener(this);
	addInputBtn.addListener(this);
	closeButton.addListener(this);

    for (int i = 0; i < 20; ++i) {
        auto* knob = new HSlider(repo, "#" + String(i + 1), "Utility parameter " + String(i + 1), true);
        paramGroup->addSlider(knob);
		addAndMakeVisible(knob);
	}

	paramGroup->listenToKnobs();
}

void ModMatrixPanel::init() {
	audio 	   = &getObj(SynthAudioSource);
	morphPanel = &getObj(MorphPanel);
	lockImage  = getObj(MiscGraphics).getIcon(8, 8);

//	addKeyListener(&getObj(KeyboardInputHandler));
}

void ModMatrixPanel::scrollBarMoved(ScrollBar* bar, double newRange) {
    TableListBox& listbox = modMatrix.getListbox();

    if (bar == &listbox.getHorizontalScrollBar()) {
		// TODO
    } else if (bar == &listbox.getVerticalScrollBar()) {
    	// TODO
    }

    repaint();
}


void ModMatrixPanel::paint(Graphics& g) {
    getObj(CycleGraphicsUtils).fillBlackground(this, g);

	g.setColour(Colours::darkgrey);
	g.setFont(16);

	Viewport& port = *modMatrix.getListbox().getViewport();

	Rectangle<int> modBounds = modMatrix.getBounds();

    g.setFont(20);
    g.drawFittedText("Destination", modBounds.getX(),
                     20, modBounds.getWidth(), 20, Justification::centred, 1);

    getObj(MiscGraphics).drawTransformedText(g, "Source",
                                             AffineTransform::rotation(-MathConstants<float>::pi * 0.5).translated(
	                                             35, modBounds.getCentreY()));

    getObj(MiscGraphics).drawTransformedText(g, "Utility Params",
                                             AffineTransform::rotation(-MathConstants<float>::pi * 0.5).translated(
	                                             getWidth() - 120, modBounds.getCentreY()));

    g.setFont(16);
	g.setColour(Colours::grey);

	{
		Graphics::ScopedSaveState sss(g);
		g.reduceClipRegion(10, modMatrix.getY(), modMatrix.getX() - 10, modMatrix.getHeight());

		for(int i = 0; i < inputs.size(); ++i)
			g.drawText(inputs.getReference(i).name, 0, modMatrix.getY() + 10 + gridSize * i - port.getViewPositionY(),
					   modMatrix.getX() - 10, 20, Justification::right, true);
	}

	AffineTransform destXform(
			AffineTransform::rotation(- MathConstants<float>::pi * 0.2f).translated(
					modMatrix.getX() + 20 - port.getViewPositionX(), modMatrix.getY() - 6));

	g.reduceClipRegion(modMatrix.getX(), 0, modMatrix.getWidth() + 30, modMatrix.getY());

    for (int i = 0; i < outputs.size(); ++i) {
	    HeaderElement& elem = outputs.getReference(i);

	    getObj(MiscGraphics).drawTransformedText(g, elem.name, destXform);
	    destXform = destXform.translated(gridSize, 0);
    }

    if (pendingFocusGrab) {
	    grabKeyboardFocus();
	    pendingFocusGrab = false;
    }
}

void ModMatrixPanel::comboBoxChanged(ComboBox* combobox) {
}

void ModMatrixPanel::buttonClicked(Button* button) {
    if (button == &addInputBtn) {
        PopupMenu menu(getInputMenu());
    	/*
        @see show()
		int showAt (const Rectangle<int>& screenAreaToAttachTo,
                int itemIDThatMustBeVisible = 0,
                int minimumWidth = 0,
                int maximumNumColumns = 0,
                int standardItemHeight = 0,
                ModalComponentManager::Callback* callback = nullptr);
    	 */

    	auto options = PopupMenu::Options()
    		.withTargetScreenArea(addInputBtn.getScreenBounds())
    		.withItemThatMustBeVisible(1)
    		.withStandardItemHeight(24);
    	menu.showMenuAsync(options, [this](int id) {
    		if (id != 0) {
				addInput(id);
				selfSize();
			}
    	});
	} else if(button == &addDestBtn) {
        PopupMenu menu(getOutputMenu(MeshTypes));

		auto options = PopupMenu::Options()
			.withTargetScreenArea(addDestBtn.getScreenBounds())
			.withItemThatMustBeVisible(1)
			.withStandardItemHeight(24);
		menu.showMenuAsync(options, [this](int id) {
			if (id != 0) {
				addDestination(id);
				selfSize();
			}
		});
	} else if(button == &closeButton) {
        removeFromDesktop();
		setVisible(false);
	}
}

void ModMatrixPanel::resized() {
    Rectangle<int> bounds 	= getLocalBounds();
	Rectangle<int> bounds2 	= bounds;
	Rectangle<int> right 	= bounds.removeFromRight(120);
	Rectangle<int> bottom 	= bounds2.removeFromBottom (60);

	bounds.removeFromBottom	(60);
	bounds.removeFromTop	(120);
	bounds.removeFromLeft	(140);
	bounds.removeFromRight	(30);

	modMatrix.setBounds  (bounds);
	addInputBtn.setBounds(30, 30, 95, 24);
	addDestBtn.setBounds (30, 60, 95, 24);

	right.reduce(20, 20);

	int utilHeight = 13 + jmax(0, getHeight() - 400) / 20;

    for (int i = 0; i < paramGroup->getNumParams(); ++i) {
		paramGroup->getKnob<Knob>(i)->setBounds(right.removeFromTop(utilHeight));
		right.removeFromTop(4);
	}

	Rectangle utilityRect(paramGroup->getKnob<Slider>(0)->getPosition(),
						  paramGroup->getKnob<Slider>(paramGroup->getNumParams() - 1)->getBounds().getBottomRight());

	utilityArea.toBack();
	utilityArea.setBounds(utilityRect);

	sourceArea.toBack();
	sourceArea.setBounds(Rectangle(juce::Point(20, bounds.getY()), bounds.getBottomLeft()));

	destArea.toBack();
	destArea.setBounds(Rectangle(juce::Point(bounds.getX(), 20), bounds.getTopRight()));

	bottom.reduce(bottom.getWidth() / 2 - 60, 12);
	closeButton.setBounds(bottom);
}

void ModMatrixPanel::addDestination(int id, bool update) {
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.getReference(i).id == id) {
			showConsoleMsg("Output already available");
			return;
		}
	}

	int flags = TableHeaderComponent::visible |
				TableHeaderComponent::notResizableOrSortable;

	String destName(getOutputName(id));

	TableHeaderComponent& header = modMatrix.getListbox().getHeader();
	header.addColumn(destName, id, gridSize, gridSize, gridSize, flags);

	outputs.add(HeaderElement(destName, id));

	if(id < VolEnvId)
		mappings.add(Mapping(VoiceTime, id, YellowDim));

	mappings.add(Mapping(KeyScale, 		 id, RedDim));
	mappings.add(Mapping(dfltModMapping, id, BlueDim));

//	dfltModMapping = id;

    if (update) {
        modMatrix.getListbox().updateContent();
		repaint();
	}
}

void ModMatrixPanel::addInput(int id, bool update) {
    for (int i = 0; i < inputs.size(); ++i) {
        if (inputs.getReference(i).id == id) {
            showConsoleMsg("Input already available");
            return;
        }
    }

    HeaderElement elem(getInputName(id), id);
    elem.shortName = getInputShortName(id);

    inputs.add(elem);

    if (update) {
        modMatrix.getListbox().updateContent();
        repaint();

        getObj(MorphPanel).setRedBlueStrings(getDimString(RedDim), getDimString(BlueDim));
	}
}

String ModMatrixPanel::getInputName(int id) {
    switch (id) {
        case VoiceTime: 	return "Voice Time";
		case Velocity: 		return "1-Velocity";
		case KeyScale: 		return "Key Scale";
		case ModWheel: 		return "Mod Wheel";
		case Aftertouch:	return "Aftertouch";
    	default: break;
	}

    if (id >= MidiController && id < Utility) {
        return "CC #" + String(id - MidiController); //+ " " + name;
    }
	if (id >= Utility) {
        return "Utility #" + String(id - Utility + 1);
    }

	return {};
}

String ModMatrixPanel::getInputShortName(int id) {
    switch (id) {
        case VoiceTime: 	return "Time";
		case Velocity: 		return "1-Vel";
		case KeyScale: 		return "Key";
		case ModWheel: 		return "Mod W";
		case Aftertouch:	return "Af.tch";
    	default: break;
	}

    if (id >= MidiController && id < Utility) {
        return "CC" + String(id - MidiController); //+ " " + name;
    }
	if (id >= Utility) {
        return "Util #" + String(id - Utility + 1);
    }

    return {};
}

String ModMatrixPanel::getOutputName(int id) {
    if (id >= TimeSurfId && id < HarmMagId) {
        return "Waveshape-L" + String(1 + (id - TimeSurfId) / 3);
    }
    if (id >= HarmMagId && id < HarmPhsId) {
	    return "SpectMag-L" + String(1 + (id - HarmMagId) / 3);
    }
		if (id >= HarmPhsId && id < VolEnvId) {
	    return "SpectPhs-L" + String(1 + (id - HarmPhsId) / 3);
    }
	if (id == VolEnvId) {
	    return "Env-Volume";
    }
	if (id == PitchEnvId) {
	    return "Env-Pitch";
    }
	if (id >= ScratchEnvId) {
	    return "Env-Scratch-" + String(1 + (id - ScratchEnvId) / 2);
    }

    return {};
}

void ModMatrixPanel::addToMenu(int id, PopupMenu& menu) {
    menu.addItem(id, getInputName(id));
}

PopupMenu ModMatrixPanel::getInputMenu() {
	PopupMenu menu;

	int inputIds[] = { VoiceTime, Velocity, KeyScale, ModWheel, Aftertouch };

    for (int inputId : inputIds) {
        if (isNotInInputs(inputId))
            addToMenu(inputId, menu);
    }

    std::set<int> midiIds;
    for (auto && input : inputs) {
        int id = input.id;
		if(id >= MidiController && id < Utility) {
			midiIds.insert(input.id);
		}
	}

	int count = 0;
    for (int i = 0; i < 10; ++i) {
        PopupMenu midiCC;

        int start = count, end = count + 9;

        for (int j = 0; j < 10; ++j) {
			int midiId = MidiController + count;

			if(midiIds.find(midiId) == midiIds.end())
				addToMenu(midiId, midiCC);

			++count;
		}

		menu.addSubMenu("MidiCC " + String(start) + "-" + String(end), midiCC);
	}

	PopupMenu utilityMenu;

	for(int i = 0; i < 20; ++i) {
        int midiId = Utility + i;

		if(midiIds.find(midiId) == midiIds.end())
			addToMenu(midiId, utilityMenu);
	}

	menu.addSubMenu("Utility", utilityMenu);

	return menu;
}

bool ModMatrixPanel::isNotInInputs(int id) {
    bool contains = false;
    for (int j = 0; j < inputs.size(); ++j) {
        if (inputs.getReference(j).id == id) {
			contains = true;
			break;
		}
	}

	return ! contains;
}

bool ModMatrixPanel::isNotInOutputs(int id) {
    bool contains = false;
    for (int j = 0; j < outputs.size(); ++j) {
        if (outputs.getReference(j).id == id) {
			contains = true;
			break;
		}
	}

	return ! contains;
}

PopupMenu ModMatrixPanel::getOutputMenu(int source) {
	PopupMenu menu;

	auto& meshLib = getObj(MeshLibrary);

	switch (source) {
        case MeshTypes:
			menu.addSubMenu("Waveshape", 	getOutputMenu(WaveshapeLayers));
			menu.addSubMenu("Spectral", 	getOutputMenu(SpectTypes));
			menu.addSubMenu("Envelopes", 	getOutputMenu(EnvTypes));
			break;

        case SpectTypes: {
            menu.addSubMenu("Magn. Layers", getOutputMenu(HarmMagLayers));
			menu.addSubMenu("Phase Layers", getOutputMenu(HarmPhsLayers));
			break;
		}

        case WaveshapeLayers: {
            for (int i = 0; i < meshLib.getLayerGroup(LayerGroups::GroupTime).size(); ++i) {
                int id = TimeSurfId + i * 3;

				if(isNotInOutputs(id)) {
					menu.addItem(id, "Layer " + String(i + 1));
				}
			}

			break;
		}
        case HarmMagLayers: {
            for (int i = 0; i < meshLib.getLayerGroup(LayerGroups::GroupSpect).size(); ++i) {
                int id = HarmMagId + i * 3;

				if(isNotInOutputs(id))
					menu.addItem(id, "Layer " + String(i + 1));
			}

			break;
		}
        case HarmPhsLayers: {
            for (int i = 0; i < meshLib.getLayerGroup(LayerGroups::GroupPhase).size(); ++i) {
                int id = HarmPhsId + i * 3;

				if(isNotInOutputs(id)) {
					menu.addItem(id, "Layer " + String(i + 1));
				}
			}

			break;
		}

		case VolumeEnvLayers: {
            for (int i = 0; i < meshLib.getLayerGroup(LayerGroups::GroupVolume).size(); ++i) {
                int id = VolEnvId + i * 3;

				if(isNotInOutputs(id)) {
					menu.addItem(id, "Volume Env " + String(i + 1));
				}
			}
		}

        case PitchEnvLayers: {
            for (int i = 0; i < meshLib.getLayerGroup(LayerGroups::GroupPitch).size(); ++i) {
                int id = PitchEnvId + i * 3;

				if(isNotInOutputs(id)) {
					menu.addItem(id, "Volume Env " + String(i + 1));
				}
			}
		}

        case ScratchLayers: {
            for (int i = 0; i < meshLib.getLayerGroup(LayerGroups::GroupScratch).size(); ++i) {
                int id = ScratchEnvId + i * 3;

                if (isNotInOutputs(id)) {
	                menu.addItem(id, "Channel " + String(i + 1));
                }
			}

			break;
		}

		case EnvTypes:
			menu.addSubMenu("Volume Envs", 	getOutputMenu(VolumeEnvLayers));
			menu.addSubMenu("Pitch Envs", 	getOutputMenu(PitchEnvLayers));
			menu.addSubMenu("Scratch Envs", getOutputMenu(ScratchLayers));

			break;

		default:
			break;
	}

	return menu;
}

int ModMatrixPanel::getMappingOrder(int outputId) {
    if (outputId >= VolEnvId) {
	    return 2;
    }

    return 3;
}

//
//ModMatrixPanel::MenuIds ModMatrixPanel::outputIdToType(int destId)
//{
//	if(destId >= TimeSurfId && destId < HarmMagId)
//		return WaveshapeTypes;
//
//	if(destId >= HarmMagId && destId < VolEnvId)
//		return SpectTypes;
//
//	if(destId >= VolEnvId && destId < PitchEnvId)
//		return VolumeEnvLayers;
//
//	return MeshTypes;
//}


int ModMatrix::columnIdToIndex(int columnId) {
    TableHeaderComponent& header = getListbox().getHeader();

    for (int i = 0; i < header.getNumColumns(true); ++i) {
        if (columnId == header.getColumnIdOfIndex(i, true)) {
	        return i;
        }
	}

	return -1;
}

void ModMatrix::ColourCheckbox::paint(Graphics& g) {
    ModMatrixPanel* panel = modMatrix->panel;
    Rectangle<int> rect 	= getLocalBounds().reduced(6, 6);

	int inputId, outputId;
	panel->getIds(row, col, inputId, outputId);

	int mappingIndex = panel->indexOfMapping(inputId, outputId);
	int dim = mappingIndex < 0 ? -1 : panel->mappings.getReference(mappingIndex).dim;

	g.fillAll(Colour::greyLevel((row + col) % 2 == 0 ? 0.1f : 0.07f));
	g.setColour(Colour::greyLevel(0.2f));
	g.drawVerticalLine(0, 0, getHeight());
	g.drawHorizontalLine(0, 0, getWidth());

	if(row == panel->inputs.size() - 1)
		g.drawHorizontalLine(getHeight() - 1, 0, getWidth());

	if(col == panel->outputs.size() - 1)
		g.drawVerticalLine(getWidth() - 1, 0, getHeight());

	float h = 0, s = 0.75f, v = 0.5f;
	Colour colour;

	switch(dim) {
		case ModMatrixPanel::YellowDim:
			h = 0.11f;
			break;

		case ModMatrixPanel::RedDim:
			h = 0.98f;
			s += 0.1f;
			v -= 0.2f;
			break;

		case ModMatrixPanel::BlueDim:
			h = 0.6f;
			break;

		case -1:
			return;
		default: break;
	}

	if(isMouseButtonDown() && dim != ModMatrixPanel::YellowDim) {
        v -= 0.1f;
    }

	g.setGradientFill(ColourGradient(Colour(h, s - 0.2, v + 0.2f, 1.f), 0, 0, Colour(h, s, v, 1.f),
	                                 ModMatrixPanel::gridSize, ModMatrixPanel::gridSize, true));
	g.fillRect(rect);

	if(dim == ModMatrixPanel::YellowDim) {
		g.drawImageAt(panel->lockImage, 18, 16);
	}
}

void ModMatrixPanel::writeXML(XmlElement* element) const {
    auto* matrixElem = new XmlElement("ModMatrix");
	auto* outputsElem = new XmlElement("Outputs");
	auto* inputsElem = new XmlElement("Inputs");
	auto* mappingsElem = new XmlElement("Mappings");

	for (auto& elem : inputs) {
		auto* inElem = new XmlElement("input");

		inElem->setAttribute("id", elem.id);
		inputsElem->addChildElement(inElem);
	}

	matrixElem->addChildElement(inputsElem);

	for (auto& elem : outputs) {
		auto* outElem = new XmlElement("output");

		outElem->setAttribute("id", elem.id);
		outputsElem->addChildElement(outElem);
	}

	matrixElem->addChildElement(outputsElem);

	for (auto& m : mappings) {
		auto* mappingElem = new XmlElement("mapping");

		mappingElem->setAttribute("in", m.in);
		mappingElem->setAttribute("out", m.out);
		mappingElem->setAttribute("dim", m.dim);

		mappingsElem->addChildElement(mappingElem);
	}

    matrixElem->addChildElement(mappingsElem);
    element->addChildElement(matrixElem);
}

void ModMatrixPanel::layerAdded(int type, int index) {
    layerAddedOrRemoved(true, type, index);
}

void ModMatrixPanel::layerRemoved(int type, int index) {
    layerAddedOrRemoved(false, type, index);
}

void ModMatrixPanel::layerAddedOrRemoved(bool added, int type, int index) {
    int outputId = -1;

    switch (type) {
		case LayerGroups::GroupTime:	outputId = TimeSurfId 	+ index * 3; break;
		case LayerGroups::GroupSpect:	outputId = HarmMagId	+ index * 3; break;
		case LayerGroups::GroupPhase:	outputId = HarmPhsId 	+ index * 3; break;
		case LayerGroups::GroupVolume:	outputId = VolEnvId 	+ index * 3; break;
		case LayerGroups::GroupPitch:	outputId = PitchEnvId 	+ index * 3; break;
		case LayerGroups::GroupScratch:	outputId = ScratchEnvId + index * 3; break;
    	default: break;
	}

	if(outputId == -1) {
		return;
	}

	Array<int> toRemove;

    if (added) {
        ScopedLock sl(mappingLock);

        addDestination(outputId, false);
    } else {
        int outIdx = -1;
        for (int i = 0; i < outputs.size(); ++i) {
            if (outputs.getReference(i).id == outputId) {
                outIdx = i;
                break;
            }
        }

        if (outIdx < 0) {
	        return;
        }

        outputs.remove(outIdx);

        TableHeaderComponent& header = modMatrix.getListbox().getHeader();
        header.removeColumn(outputId);

        for (int j = 0; j < mappings.size(); ++j) {
            Mapping& m = mappings.getReference(j);

			if(m.out == outputId) {
				toRemove.add(j);
			}
		}
	}

    if (toRemove.size() > 0 || added) {
        {
            ScopedLock sl(mappingLock);

            // do it backwards to keep indices valid
			for(int i = toRemove.size() - 1; i >= 0; --i) {
				mappings.remove(toRemove[i]);
			}
		}

		modMatrix.getListbox().updateContent();
	}
}

juce::Component* ModMatrixPanel::getComponent(int which) {
    switch (which) {
        case CycleTour::TargMatrixSource: 		return &sourceArea;
		case CycleTour::TargMatrixAddDest: 		return &addDestBtn;
		case CycleTour::TargMatrixAddSource: 	return &addInputBtn;
		case CycleTour::TargMatrixDest: 		return &destArea;
		case CycleTour::TargMatrixGrid: 		return &modMatrix;
		case CycleTour::TargMatrixUtility: 		return &utilityArea;
    	default: break;
	}

	return nullptr;
}

//bool ModMatrixPanel::keyPressed(const KeyPress& key, Component* component)
bool ModMatrixPanel::keyPressed(const KeyPress& press) {
    return getObj(KeyboardInputHandler).keyPressed(press, this);
}

bool ModMatrixPanel::readXML(const XmlElement* element) {
    XmlElement* matrixElem = element->getChildByName("ModMatrix");

    if (matrixElem == nullptr) {
        initializeDefaults();
        return false;
    }

    XmlElement* inputsElem = matrixElem->getChildByName("Inputs");
    XmlElement* outputsElem = matrixElem->getChildByName("Outputs");
    XmlElement* mappingsElem = matrixElem->getChildByName("Mappings");

    if (inputsElem == nullptr || outputsElem == nullptr || mappingsElem == nullptr) {
        initializeDefaults();

		return false;
	}

	inputs.clear();
	outputs.clear();
	mappings.clear();

    // inputs

    for(auto inputElem : inputsElem->getChildWithTagNameIterator("input")) {
	    int id = inputElem->getIntAttribute("id", -1);

	    HeaderElement elem(getInputName(id), id);
	    elem.shortName = getInputShortName(id);

	    inputs.add(elem);
    }

    // outputs

    TableHeaderComponent& header = modMatrix.getListbox().getHeader();
    header.removeAllColumns();
    int flags = TableHeaderComponent::visible | TableHeaderComponent::notResizableOrSortable;

    //	header.addColumn("Set All", SetAllId, gridSize, gridSize, gridSize, flags);

    for(auto outputElem : outputsElem->getChildWithTagNameIterator("output")) {
	    int id = outputElem->getIntAttribute("id", -1);

	    HeaderElement elem(getOutputName(id), id);
	    outputs.add(elem);

	    header.addColumn(elem.name, id, gridSize, gridSize, gridSize, flags);
    }

    // mappings

    for(auto mappingElem : mappingsElem->getChildWithTagNameIterator("mapping")) {
	    Mapping m;
	    m.in = mappingElem->getIntAttribute("in", -1);
	    m.out = mappingElem->getIntAttribute("out", -1);
	    m.dim = mappingElem->getIntAttribute("dim", -1);

	    mappings.add(m);
    }

    modMatrix.getListbox().updateContent();
    getObj(MorphPanel).setRedBlueStrings(getDimString(RedDim), getDimString(BlueDim));

	return true;
}

int ModMatrixPanel::indexOfMapping(int inputId, int outputId) {
    int mappingIndex = -1;

	if(outputId == SetAllId)
		return -1;

    for (int i = 0; i < mappings.size(); ++i) {
        Mapping& m = mappings.getReference(i);
        if (m.in == inputId && m.out == outputId) {
			mappingIndex = i;
			break;
		}
	}

	return mappingIndex;
}

void ModMatrixPanel::getIds(int row, int col, int& inputId, int& outputId) {
    inputId = inputs.getReference(row).id;
//	outputId = col == 0 ? SetAllId : outputs.getReference(col - 1).id;
    outputId = outputs.getReference(col).id;
}

void ModMatrixPanel::cycleDimensionMapping(int row, int col) {
    int inputId, outputId;
    getIds(row, col, inputId, outputId);

	int dim, mappingIndex;

	bool isVoiceTime = inputId == VoiceTime;
	bool isEnvelope = outputId >= VolEnvId;

	mappingIndex = indexOfMapping(inputId, outputId);
	dim = mappingIndex < 0 ? NullDim : mappings.getReference(mappingIndex).dim;

	if(dim == YellowDim || (isVoiceTime && isEnvelope)) {
		return;
	}

	int old = dim;

	// skip time
	if(dim == NullDim) {
		dim = RedDim;
	} else {
		++dim;
	}

	if(dim > BlueDim) {
		dim = NullDim;
	}

	mappingChanged(mappingIndex, inputId, outputId, old, dim);
}

void ModMatrixPanel::mappingChanged(int mappingIndex, int inputId, int outputId, int oldDim, int newDim) {
    getObj(EditWatcher).setHaveEditedWithoutUndo(true);

	// remove old when cycled out
    if (mappingIndex >= 0 && newDim == -1) {
		ScopedLock sl(mappingLock);

		mappings.remove(mappingIndex);
		return;
	}

    // new mapping
    if (mappingIndex < 0) {
        ScopedLock sl(mappingLock);
		mappings.add(Mapping(inputId, outputId, 0));
		mappingIndex = mappings.size() - 1;
	}

	Mapping& m = mappings.getReference(mappingIndex);

    // update old mapping
    if (newDim != -1) {
		m.dim = newDim;
	}

    // can have no duplicates of a certain dim per output
    Array<int> toRemove;
    for (int i = 0; i < mappings.size(); ++i) {
        Mapping& m2 = mappings.getReference(i);
        if (i != mappingIndex && m2.out == m.out && m2.dim == m.dim) {
            toRemove.add(i);
		}
	}

    if (toRemove.size() > 0) {
        {
            ScopedLock sl(mappingLock);
            for (int i = toRemove.size() - 1; i >= 0; --i) {
                mappings.remove(toRemove[i]);
			}
		}

		modMatrix.getListbox().updateContent();
		getObj(MorphPanel).setRedBlueStrings(getDimString(RedDim), getDimString(BlueDim));
	}
}

void ModMatrixPanel::route(float value, int inputId, int voiceIndex) {
    ScopedLock sl(mappingLock);

    for (int i = 0; i < mappings.size(); ++i) {
        Mapping& m = mappings.getReference(i);

        if (m.in == inputId) {
            audio->modulationChanged(value, voiceIndex, m.out, m.dim);

            if (m.dim == BlueDim) {
	            morphPanel->blueDimUpdated(value);
            }

			if(m.dim == RedDim) {
				morphPanel->redDimUpdated(value);
			}
        }
    }
}

void ModMatrixPanel::initializeDefaults() {
	TableHeaderComponent& header = modMatrix.getListbox().getHeader();
	header.removeAllColumns();
	outputs.clear();
	inputs.clear();
	mappings.clear();

	auto& lib = getObj(MeshLibrary);

	using namespace LayerGroups;
	int groups[] = { GroupTime,  GroupSpect, GroupPhase, GroupVolume, GroupPitch, GroupScratch };
	int ids[] 	 = { TimeSurfId, HarmMagId,  HarmPhsId,  VolEnvId, 	  PitchEnvId, ScratchEnvId };

    for (int i = 0; i < numElementsInArray(groups); ++i) {
        for (int j = 0; j < lib.getLayerGroup(groups[i]).size(); ++j)
            addDestination(ids[j] + 3 * i, false);
    }

	addInput(VoiceTime,  false);
	addInput(Velocity, 	 false);
	addInput(KeyScale, 	 false);
	addInput(ModWheel, 	 false);
	addInput(Aftertouch, false);

	getObj(MorphPanel).setRedBlueStrings(getDimString(RedDim), getDimString(BlueDim));

	modMatrix.getListbox().updateContent();
}

void ModMatrixPanel::selfSize() {
    setSize(jmin(800, 550 + (outputs.size() - 6) * gridSize),
            jlimit(390, 800, 400 + (inputs.size() - 6) * gridSize));
}

String ModMatrixPanel::getDimString(int dim) {
    StringArray str;

    for (int i = 0; i < mappings.size(); ++i) {
        Mapping& mapping = mappings.getReference(i);

        if (mapping.dim == dim) {
            for (int i = 0; i < inputs.size(); ++i) {
                HeaderElement input = inputs.getReference(i);

				if(input.id == mapping.in)
					str.addIfNotAlreadyThere(input.shortName);
			}
		}
	}

    if (str.size() == 0) {
        return dim == RedDim ? "red" : "blue";
    }

	return str.joinIntoString(",");
}

float ModMatrixPanel::getUtilityValue(int utilityIndex) {
    return paramGroup->getKnobValue(utilityIndex);
}

void ModMatrixPanel::setUtilityValue(int utilityIndex, float value) {
    paramGroup->setKnobValue(utilityIndex, value, false, true);
}

bool ModMatrixPanel::shouldTriggerGlobalUpdate(Slider* slider) {
    return false;
}

bool ModMatrixPanel::shouldTriggerLocalUpdate(Slider* slider) {
    return false;
}
bool ModMatrixPanel::updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) {
    route(knobValue, Utility + knobIndex);
    return true;
}

void ModMatrixPanel::setMatrixCell(int inputId, int outputId, int dim) {
    if (dim == YellowDim)
        return;

    int mappingIndex = indexOfMapping(inputId, outputId);
    int oldDim = mappingIndex < 0 ? -1 : mappings.getReference(mappingIndex).dim;

    if (oldDim != dim)
        mappingChanged(mappingIndex, inputId, outputId, oldDim, dim);
}

void ModMatrixPanel::setPendingFocusGrab(bool val) {
  #if JUCE_MAC
    (new Message())->post();
  #endif

    pendingFocusGrab = val;
}

void ModMatrixPanel::handleMessage(const Message& message) {
  #if JUCE_MAC
    if(! hasKeyboardFocus(false))
        postMessage(new Message());

    grabKeyboardFocus();
  #endif
}

MeshLibrary::GroupLayerPair ModMatrixPanel::toLayerIndex(int outputId) {
    int numEnvelopeDims = getObj(SynthAudioSource).getNumEnvelopeDims();
    int groupId = CommonEnums::Null;
    int layerIdx = CommonEnums::Null;

    if (NumberUtils::withinExclUpper(outputId, (int) HarmMagId, (int) HarmPhsId)) {
        layerIdx = (outputId - HarmMagId) / 3;
        groupId = LayerGroups::GroupSpect;
    } else if (NumberUtils::withinExclUpper(outputId, (int) HarmPhsId, (int) VolEnvId)) {
        layerIdx = (outputId - HarmPhsId) / 3;
        groupId = LayerGroups::GroupPhase;
    } else if (NumberUtils::withinExclUpper(outputId, (int) TimeSurfId, (int) HarmMagId)) {
        layerIdx = (outputId - TimeSurfId) / 3;
        groupId = LayerGroups::GroupTime;
    } else if (NumberUtils::withinExclUpper(outputId, (int) VolEnvId, (int) PitchEnvId)) {
        layerIdx = (outputId - VolEnvId) / numEnvelopeDims;
        groupId = LayerGroups::GroupVolume;
    } else if (NumberUtils::withinExclUpper(outputId, (int) PitchEnvId, (int) ScratchEnvId)) {
        layerIdx = (outputId - PitchEnvId) / numEnvelopeDims;
        groupId = LayerGroups::GroupPitch;
    } else if (NumberUtils::withinExclUpper(outputId, (int) ScratchEnvId, (int) NextId)) {
        layerIdx = (outputId - ScratchEnvId) / numEnvelopeDims;
        groupId = LayerGroups::GroupScratch;
    }

    return {groupId, layerIdx};
}

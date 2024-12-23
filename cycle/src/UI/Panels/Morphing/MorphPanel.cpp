#include <App/AppConstants.h>
#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/Multisample.h>
#include <Curve/VertCube.h>
#include <Definitions.h>
#include <Design/Updating/Updater.h>
#include <Inter/Interactor.h>
#include <UI/Widgets/CalloutUtils.h>
#include <UI/Widgets/IconButton.h>
#include <Util/Arithmetic.h>

#include "CubeDisplay.h"
#include "MorphPanel.h"

#include "../CycleDefs.h"
#include "../GeneralControls.h"
#include "../MainPanel.h"
#include "../ModMatrixPanel.h"
#include "../PlaybackPanel.h"
#include "../VertexPropertiesPanel.h"
#include "../../CycleGraphicsUtils.h"
#include "../../Widgets/HSlider.h"
#include "../../Widgets/MorphSlider.h"
#include "../../Widgets/MidiKeyboard.h"
#include "../../../App/CycleTour.h"
#include "../../VisualDsp.h"

MorphPanel::MorphPanel(SingletonRepo* repo) :
        SingletonAccessor(repo, "MorphPanel")
    ,	primeYllw(	1, 1, this, repo, "Set time range as primary axis", "1")
    ,	primeRed(	0, 1, this, repo, "Set key scale range as primary axis", "2")
    ,	primeBlue(	2, 1, this, repo, "Set modulation range as primary axis", "3")
    ,	linkYllw(	1, 1, this, repo, "Move the ends of selected vert's time range in unison", "t, 7")
    ,	linkRed(	0, 1, this, repo, "Move the ends of selected vert's red range in unison", "k, 8")
    ,	linkBlue(	2, 1, this, repo, "Move the ends of selected vert's blue range in unison", "m, 9")
    ,	rangeYllw(	1, 1, this, repo, "Use slider's position & depth for time-span of new vertex cubes")
    ,	rangeRed(	0, 1, this, repo, "Use slider's position & depth for red-range span of new vertex cubes")
    ,	rangeBlue(	2, 1, this, repo, "Use slider's position & depth for blue-range span of new vertex cubes")
    ,	yllwSlider(	repo, "TIME", "Time range view position", true)
    ,	redSlider(	repo, "Red",  "Red range view position", true)
    ,	blueSlider(	repo, "Blue","Blue range view position", true)
    ,	panSlider(	repo, "PAN",  "Pan view position (0 = left, 1 = right)", true)
    ,	ignoreNextEditMessage(false) {
    yllwSlider.dim = Vertex::Time;
    redSlider.dim  = Vertex::Red;
    blueSlider.dim = Vertex::Blue;

    cubeDisplay = std::make_unique<CubeDisplay>(repo);
    addAndMakeVisible(cubeDisplay.get());

//	cube = PNGImageFormat().loadFrom(CycleImages::cube_png, CycleImages::cube_pngSize);

    HSlider* sliders[] = { &blueSlider, &redSlider, &yllwSlider, &panSlider };

    Colour yellow = Color(1.f, 0.8f, 0.27f).toColour();

    for (auto & slider : sliders) {
        slider->addListener(this);
        slider->setRange(0, 1);
        slider->setValue(0, dontSendNotification);
        slider->setColour(yellow);

        addAndMakeVisible(slider);
    }

    StringFunction simpleRounder = StringFunction(2);

    blueSlider.setStringFunction(simpleRounder);
    redSlider.setStringFunction(simpleRounder);
    yllwSlider.setStringFunction(simpleRounder);

    redSlider.setValue(Arithmetic::getUnitValueForGraphicNote(60, midiRange), dontSendNotification);
    panSlider.setValue(0.5f, dontSendNotification);
    panSlider.setColour(Colour::greyLevel(0.8f));

    viewDepth[Vertex::Time] = 1.f;
    viewDepth[Vertex::Red] 	= 1.f;
    viewDepth[Vertex::Blue] = 1.f;

    insertDepth[Vertex::Time] = 1.f;
    insertDepth[Vertex::Red] = 1.f;
    insertDepth[Vertex::Blue] = 1.f;

    addAndMakeVisible(&slidersArea);

    Component* rangeTemp[] = { &rangeYllw, &rangeRed, &rangeBlue };
    CalloutUtils::addRetractableCallout(rangeCO, rangePO, repo, 0, 1, rangeTemp,
                                        numElementsInArray(rangeTemp), this, true);

    Component* dimTemp[] = { &primeYllw, &primeRed, &primeBlue };
    CalloutUtils::addRetractableCallout(dimCO, dimPO, repo, 0, 1, dimTemp,
                                        numElementsInArray(dimTemp), this, true);

    Component* linkTemp[] = { &linkYllw, &linkRed, &linkBlue };
    CalloutUtils::addRetractableCallout(linkCO, linkPO, repo, 0, 1, linkTemp,
                                        numElementsInArray(linkTemp), this, true);

    setWantsKeyboardFocus(false);
}

void MorphPanel::setDepth(int dim, float depth) {
    viewDepth[dim] = depth;
    insertDepth[dim] = depth;
}

void MorphPanel::init() {
//	mappingBox.setSelectedId(ModMappingId, true);

    midiRange = Range<int>(getConstant(LowestMidiNote), getConstant(HighestMidiNote));
    updateHighlights();
}

void MorphPanel::paint(Graphics& g) {
    getObj(CycleGraphicsUtils).fillBlackground(this, g);

    g.setFont(*getObj(MiscGraphics).getVerdana12());
    g.setColour(Colour::greyLevel(0.6f));


    Rectangle gradBounds(getLocalBounds().removeFromRight(140).removeFromTop(120));

    {
        g.setGradientFill(ColourGradient(Colour::greyLevel(0.05f),
                          gradBounds.getRight() - 10, gradBounds.getY(),
                          Colour::greyLevel(0.05f).withAlpha(0.f),
                          gradBounds.getX(), gradBounds.getY(), true));

        g.fillRect(gradBounds);
    }

    String positionStr("morph Position");

    Font* silkscreen = getObj(MiscGraphics).getSilkscreen();
    int positionX 	 = (yllwSlider.getWidth() - Util::getStringWidth(*silkscreen, positionStr)) / 2;

    g.setColour(Colour::greyLevel(0.55f));
    g.setFont(*silkscreen);

    getObj(MiscGraphics).drawShadowedText(g, positionStr, positionX, yllwSlider.getY() - 5, *silkscreen);

    Rectangle<int> rects[] = {
            Rectangle<int>(redSlider.getRight()  + 2, redSlider.getY(),  4, redSlider.getHeight()),
            Rectangle<int>(yllwSlider.getRight() + 2, yllwSlider.getY(), 4, yllwSlider.getHeight()),
            Rectangle<int>(blueSlider.getRight()  + 2, blueSlider.getY(),  4, blueSlider.getHeight())
    };

    Colour colours[] = {
            Colour(0.95f, 0.7f, 0.4f, 0.7f),
            Colour(0.11f, 0.7f, 0.45f, 0.7f),
            Colour(0.65f, 0.7f, 0.65f, 0.7f)
    };

    for (int i = 0; i < numElementsInArray(rects); ++i) {
        g.setColour(colours[i]);
        g.fillRect(rects[i]);
        g.setColour(colours[i].brighter(0.05f));
        g.drawRect(rects[i]);
    }

    getObj(MiscGraphics).drawJustifiedText(g, "x axis", 	primeYllw, 	primeBlue, 	true, dimCO.get());
    getObj(MiscGraphics).drawJustifiedText(g, "link", 		linkYllw, linkBlue, 	true, linkCO.get());
    getObj(MiscGraphics).drawJustifiedText(g, "cube range", rangeYllw, 	rangeBlue, 	true, rangeCO.get());
}

float MorphPanel::getValue(int index) {
    switch (index) {
        case Vertex::Time: 	return (float) yllwSlider.getValue();
        case Vertex::Red: 	return (float) redSlider.getValue();
        case Vertex::Blue: 	return (float) blueSlider.getValue();

        default: return 0;
    }
}

MorphPosition MorphPanel::getMorphPosition() {
    return {(float) yllwSlider.getValue(), (float) redSlider.getValue(), (float) blueSlider.getValue()};
}

MorphPosition MorphPanel::getOffsetPosition(bool includingDepths) {
    MorphPosition m;

    m.time = getSetting(UseYellowDepth) ? yllwSlider.getValue() : 0.f;
    m.red  = getSetting(UseRedDepth)    ? redSlider.getValue()  : 0.f;
    m.blue = getSetting(UseBlueDepth)   ? blueSlider.getValue() : 0.f;

    if (includingDepths) {
        m.timeDepth = getSetting(UseYellowDepth) ? viewDepth[Vertex::Time] : 1.f;
        m.redDepth  = getSetting(UseRedDepth)    ? viewDepth[Vertex::Red]  : 1.f;
        m.blueDepth = getSetting(UseBlueDepth)   ? viewDepth[Vertex::Blue] : 1.f;
    }

    return m;
}

void MorphPanel::resized() {
    Rectangle<int> bounds = getBounds().withPosition(0, 0);
    bounds.reduce(5, 5);

    Rectangle<int> sliderBounds = bounds;

    Rectangle<int> iconBounds = sliderBounds.removeFromBottom(45);
    cubeBounds = sliderBounds.removeFromRight(130).expanded(10, 10);
    cubeBounds.removeFromLeft(15);
    cubeDisplay->setBounds(cubeBounds);
    sliderBounds.removeFromTop(13);

    Slider* sliders[] 	= { &yllwSlider, &redSlider, &blueSlider, &panSlider };

    int numSliders = numElementsInArray(sliders);
    int sliderHeight = int((sliderBounds.getHeight() - (numSliders - 1) * 3) / float(numSliders));

    for (int i = 0; i < numSliders; ++i) {
        Rectangle sb(sliderBounds.removeFromTop(sliderHeight));
        sb.removeFromRight(8);

        sliders[i]->setBounds(sb);
        sliderBounds.removeFromTop(3);
    }

    iconBounds.removeFromLeft(3);
    iconBounds.removeFromTop(20);

    dimCO->setBounds(iconBounds.removeFromLeft(dimCO->getExpandedSize()));
    linkCO->setBounds(iconBounds.removeFromRight(linkCO->getExpandedSize()));
    iconBounds.reduce((iconBounds.getWidth() - rangeCO->getExpandedSize()) / 2, 0);
    rangeCO->setBounds(iconBounds);

    slidersArea.setBounds(Rectangle(yllwSlider.getPosition(), blueSlider.getBounds().getBottomRight()));
    slidersArea.toBack();
}

void MorphPanel::sliderValueChanged(Slider* slider) {
    if (slider == &panSlider) {
        if(getSetting(ViewStage) >= ViewStages::PostEnvelopes)
            triggerRefreshUpdate();

        return;
    }

    auto* mslider = dynamic_cast<MorphSlider*>(slider);

    if(mslider != nullptr)
        updateModPosition(mslider->dim, slider->getValue());
}

void MorphPanel::updateModPosition(int dim, float value) {
    if (dim == Vertex::Red) {
        getObj(MidiKeyboard).setAuditionKey(getCurrentMidiKey());
    }

    cubeDisplay->triggerAsyncUpdate();

    // just in case this event didn't come from the sliders
    setValue(dim, value);

    if (dim != getSetting(CurrentMorphAxis)) {
        if (!getSetting(DrawWave))
            triggerRefreshUpdate();
        else {
            auto& multi = getObj(Multisample);
            PitchedSample* existing = multi.getCurrentSample();

            multi.performUpdate(UpdateType::Update);

            if(existing != multi.getCurrentSample())
                triggerRefreshUpdate();
        }
    } else
        getObj(PlaybackPanel).setProgress(value, false);

    if (dim == Vertex::Blue) {
        getObj(ModMatrixPanel).route(value, ModMatrixPanel::MidiController + 1);
    }
}

void MorphPanel::sliderDragEnded(Slider* slider) {
    if (getSetting(DrawWave))
        return;

    auto* mslider = dynamic_cast<MorphSlider*>(slider);

    if (mslider || slider == &panSlider) {
        if (slider == &panSlider || mslider->dim != getSetting(CurrentMorphAxis))
        {
            triggerRestoreUpdate();
        }
    }
}

void MorphPanel::sliderDragStarted(Slider* slider) {
    if (getSetting(DrawWave)) {
        return;
    }

    auto* mslider = dynamic_cast<MorphSlider*>(slider);

    if (mslider || slider == &panSlider) {
        if (slider == &panSlider || mslider->dim != getSetting(CurrentMorphAxis)) {
            triggerReduceUpdate();
        }
    }
}

void MorphPanel::updateCurrentSliderNoCallback(float value) {
    int dim = getSetting(CurrentMorphAxis);

    if(yllwSlider.dim == dim) {
        yllwSlider.setValue(value, dontSendNotification);
    } else if(blueSlider.dim == dim) {
        blueSlider.setValue(value, dontSendNotification);
    } else if(redSlider.dim == dim) {
        redSlider.setValue(value, dontSendNotification);
    }

    cubeDisplay->triggerAsyncUpdate();
}

void MorphPanel::showMessage(float value, MorphSlider* slider) {
    if (slider == &redSlider) {
        int midiKey = Arithmetic::getNoteForValue(value, midiRange);
        String keyStr = MidiMessage::getMidiNoteName(midiKey, true, true, 3);

        showMsg(keyStr);
    } else if (slider == &blueSlider) {
        showMsg(String(value, 2));
    } else if (slider == &yllwSlider) {
        showMsg(String(value, 2));
    }
}

void MorphPanel::reduceDetail() {
    getObj(Updater).update(UpdateSources::SourceMorph, ReduceDetail);
}

void MorphPanel::restoreDetail() {
    getObj(Updater).update(UpdateSources::SourceMorph, RestoreDetail);
}

void MorphPanel::doGlobalUIUpdate(bool force) {
    getObj(Updater).update(UpdateSources::SourceMorph, Update);
}

int MorphPanel::getCurrentMidiKey() {
    return Arithmetic::getGraphicNoteForValue(redSlider.getValue(), midiRange);
}

void MorphPanel::setKeyValueForNote(int midiNote) {
    redSlider.setValue(Arithmetic::getUnitValueForGraphicNote(midiNote, midiRange), dontSendNotification);
}


void MorphPanel::buttonClicked(Button* button) {
    bool changedLinking = false;

    if (button == &primeYllw || button == &primeRed || button == &primeBlue) {
        getObj(MainPanel).setPrimaryDimension(button == &primeYllw ? Vertex::Time :
                                              button == &primeRed ? Vertex::Red : Vertex::Blue, true);
        doUpdate(SourceMorph);
    } else if (button == &linkYllw) {
        getSetting(LinkYellow) ^= true;
        changedLinking = true;
    } else if (button == &linkRed) {
        getSetting(LinkRed) ^= true;
        changedLinking = true;
    } else if (button == &linkBlue) {
        getSetting(LinkBlue) ^= true;
        changedLinking = true;
    } else if (button == &rangeYllw) {
        getSetting(UseYellowDepth) ^= true;
    } else if (button == &rangeRed) {
        getSetting(UseRedDepth) ^= true;
    } else if (button == &rangeBlue) {
        getSetting(UseBlueDepth) ^= true;
    }

    if (changedLinking) {
        Interactor* itr = getObj(VertexPropertiesPanel).getCurrentInteractor();

        if(itr != nullptr) {
            itr->setMovingVertsFromSelected();
        }

        cubeDisplay->linkingChanged();
    }

    updateHighlights();
}

void MorphPanel::updateHighlights() {
    int dim = getSetting(CurrentMorphAxis);

    primeYllw.setHighlit(dim == Vertex::Time);
    primeRed .setHighlit(dim == Vertex::Red);
    primeBlue.setHighlit(dim == Vertex::Blue);

    linkYllw.setHighlit(getSetting(LinkYellow) == 1);
    linkRed	.setHighlit(getSetting(LinkRed) == 1);
    linkBlue.setHighlit(getSetting(LinkBlue) == 1);

    getObj(GeneralControls).setLinkHighlight(getSetting(LinkYellow) == 1);

    rangeYllw.setHighlit(usesLineDepthFor(Vertex::Time));
    rangeRed.setHighlit(usesLineDepthFor(Vertex::Red));
    rangeBlue.setHighlit(usesLineDepthFor(Vertex::Blue));
}


/*
void MorphPanel::comboBoxChanged(ComboBox* box)
{
    int id = box->getSelectedId();

    if(box == &mappingBox)
    {
        if(currentModMapping != id)
        {
            currentModMapping = id;
            if(id == NullMappingId)
            {
                MorphSlider.setName("unset");
            }
            else
            {
                MorphSlider.setName(box->getText());
            }

            if(ignoreNextEditMessage)
                ignoreNextEditMessage = false;
            else
                getObj(EditWatcher).setHaveEditedWithoutUndo(true);
        }
    }
}
*/


float MorphPanel::getPrimaryViewDepth() {
    return viewDepth[getSetting(CurrentMorphAxis)];
}


float MorphPanel::getDepth(int dim) {
    return viewDepth[dim];
}


void MorphPanel::setViewDepth(int dim, float depth) {
    viewDepth[dim] = depth;
}


void MorphPanel::setValue(int dim, float value) {
    switch (dim) {
        case Vertex::Time:	yllwSlider.setValue(value, dontSendNotification); 	break;
        case Vertex::Red: 	redSlider.setValue(value, dontSendNotification);	break;
        case Vertex::Blue: 	blueSlider.setValue(value, dontSendNotification);	break;
        default: break;
    }
}


void MorphPanel::triggerValue(int dim, float value) {
    switch (dim) {
        case Vertex::Time:	yllwSlider.setValue(value, sendNotificationAsync);	break;
        case Vertex::Red: 	redSlider.setValue(value, sendNotificationAsync);	break;
        case Vertex::Blue: 	blueSlider.setValue(value, sendNotificationAsync);	break;
        default: break;
    }
}


void MorphPanel::triggerClick(int button) {
    switch (button) {
        case CycleTour::IdBttnLinkY: 	buttonClicked(&linkYllw); break;
        case CycleTour::IdBttnLinkR: 	buttonClicked(&linkRed); 	break;
        case CycleTour::IdBttnLinkB: 	buttonClicked(&linkBlue); 	break;

        case CycleTour::IdBttnRangeY: 	buttonClicked(&rangeYllw); 	break;
        case CycleTour::IdBttnRangeR: 	buttonClicked(&rangeRed); 	break;
        case CycleTour::IdBttnRangeB: 	buttonClicked(&rangeBlue); 	break;

        case CycleTour::IdBttnPrimeY: 	buttonClicked(&primeYllw); 	break;
        case CycleTour::IdBttnPrimeR: 	buttonClicked(&primeRed); 	break;
        case CycleTour::IdBttnPrimeB: 	buttonClicked(&primeBlue); 	break;
        default: break;
    }
}

bool MorphPanel::readXML(const XmlElement* element) {
    // XmlElement* modElem = element->getChildByName("ModMapping");
    //
    // if(! modElem) {
       //  return false;
    // }
    //
    // int id = modElem->getIntAttribute("modMappingId", ModMappingId);
    //
    // ignoreNextEditMessage 	= true;
    // setCurrentModMapping(id);
    // updateHighlights();

    return true;
}

void MorphPanel::writeXML(XmlElement* element) const {
//	jassert(element);
//	int id = mappingBox.getSelectedId();
}

//bool MorphPanel::isCurrentModMappingVelocity()
//{
//	return currentModMapping == VelocityMappingId || currentModMapping == InvVelocityMappingId;
//}

void MorphPanel::redDimUpdated(float value) {
    redSlider.setCurrentValue(value);
}

void MorphPanel::blueDimUpdated(float value) {
    blueSlider.setCurrentValue(value);
}

void MorphPanel::setSelectedCube(Vertex* vert, VertCube* cube, int scratchChannel, bool isEnvelope) {
    int selectedIdx = cube == nullptr ? -1 : cube->indexOf(vert);

    cubeDisplay->update(cube, selectedIdx, scratchChannel, isEnvelope);
}

void MorphPanel::updateCube() const {
    cubeDisplay->triggerAsyncUpdate();
}

void MorphPanel::setRedBlueStrings(const String& redStr, const String& blueStr) {
    redSlider.name = redStr;
    blueSlider.name = blueStr;

    redSlider.repaint();
    blueSlider.repaint();
}

Component* MorphPanel::getComponent(int which) {
    switch (which) {
        case CycleTour::TargVertCube:	return cubeDisplay.get();
        case CycleTour::TargLinkArea:	return linkCO.get();
        case CycleTour::TargLinkY:		return &linkYllw;
        case CycleTour::TargLinkR:		return &linkRed;
        case CycleTour::TargLinkB:		return &linkBlue;

        case CycleTour::TargPrimeArea:	return dimCO.get();
        case CycleTour::TargPrimeY:		return &primeYllw;
        case CycleTour::TargPrimeR:		return &primeRed;
        case CycleTour::TargPrimeB:		return &primeBlue;

        case CycleTour::TargSlidersArea:return &slidersArea;
        case CycleTour::TargSliderY:	return &yllwSlider;
        case CycleTour::TargSliderR:	return &redSlider;
        case CycleTour::TargSliderB:	return &blueSlider;
        case CycleTour::TargSliderPan:	return &panSlider;

        case CycleTour::TargRangeArea:	return rangeCO.get();
        case CycleTour::TargRangeY:		return &rangeYllw;
        case CycleTour::TargRangeR:		return &rangeRed;
        case CycleTour::TargRangeB:		return &rangeBlue;
        default: break;
    }

    return nullptr;
}

int MorphPanel::getPrimaryDimension() {
    return getSetting(CurrentMorphAxis);
}

bool MorphPanel::usesLineDepthFor(int dim) {
    switch (dim) {
        case Vertex::Time:  return getSetting(UseYellowDepth) == 1;
        case Vertex::Red:   return getSetting(UseRedDepth) == 1;
        case Vertex::Blue:  return getSetting(UseBlueDepth) == 1;
        default: break;
    }

    return false;
}

float MorphPanel::getDistortedTime(int chan) {
    return getObj(VisualDsp).getScratchPosition(chan);
}

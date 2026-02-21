#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Design/Updating/Updater.h>

#include "OscControlPanel.h"

#include <App/CycleTour.h>

#include "../CycleGraphicsUtils.h"
#include "../VertexPanels/Spectrum3D.h"
#include "../Widgets/HSlider.h"
#include "../../Audio/Effects/Unison.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Util/CycleEnums.h"
#include "../CycleDefs.h"


OscControlPanel::OscControlPanel(SingletonRepo* repo) :
        SingletonAccessor(repo, "OscControlPanel")
    ,	Worker(repo, "OscControlPanel")
{
}

void OscControlPanel::init() {
    synth = &getObj(SynthAudioSource);

    paramGroup->addSlider(volume = new HSlider(repo, "VOLUME", "Volume", false));
    paramGroup->addSlider(octave = new HSlider(repo, "OCTAVE", "Octave", false));
    paramGroup->addSlider(speed  = new HSlider(repo, "DURATION", "Duration", false));

    for (int i = 0; i < numSliders; ++i) {
        paramGroup->setKnobValue(i, 0.5, false, false);
    }

    paramGroup->listenToKnobs();

    hSliders[Volume] = volume;
    hSliders[Octave] = octave;
    hSliders[Speed]  = speed;

    using namespace Ops;

    StringFunction decibel18(StringFunction() .mul(2.0).sub(1.).mul(18.));
    StringFunction octaveStr(StringFunction(0).sub(0.5).mul(4.).add(0.5).rnd());
    StringFunction speedTime(StringFunction(0).mul(8.0).sub(3.).pow(M_E).rnd());

    volume->setStringFunction(decibel18, " dB");
    octave->setStringFunction(octaveStr, " octaves");
    speed->setStringFunction(speedTime, " seconds");

    addAndMakeVisible(volume);
    addAndMakeVisible(octave);
    addAndMakeVisible(speed);

    startTimer(100);
}

void OscControlPanel::paint(Graphics& g) {
    getObj(CycleGraphicsUtils).fillBlackground(this, g);
}

bool OscControlPanel::doUpdateWithSpeedSlider() {
    return getSetting(ViewStage) >= ViewStages::PostEnvelopes &&
           (synth->getUnison().isEnabled() || getObj(MeshLibrary).getCurrentProps(LayerGroups::GroupPitch)->active);
}

bool OscControlPanel::shouldTriggerGlobalUpdate(Slider* slider) {
    if (slider == octave || (slider == speed && !doUpdateWithSpeedSlider())) {
        return false;
    }

    return true;
}


void OscControlPanel::sliderValueChanged(Slider* slider)
{
    // update the backgrounds;
    if (slider == speed)
    {
        getObj(SynthAudioSource).updateTempoScale();

//		getObj(Updater).updateTimeBackgrounds(true); //XXX
    }

    paramGroup->sliderValueChanged(slider);
}

void OscControlPanel::restoreDetail() {
    getObj(Updater).update(UpdateSources::SourceOscCtrls, RestoreDetail);
}

void OscControlPanel::reduceDetail() {
    getObj(Updater).update(UpdateSources::SourceOscCtrls, ReduceDetail);
}

void OscControlPanel::doGlobalUIUpdate(bool force) {
    getObj(Updater).update(UpdateSources::SourceOscCtrls);
}

bool OscControlPanel::updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) {
    switch (knobIndex) {
        case Volume:
            synth->paramChanged(Synthesizer::VolumeParam, scaleVolume(knobValue));
            break;

        case Octave:
            synth->paramChanged(Synthesizer::OctaveParam, scaleOctave(knobValue));
            break;

        case Speed:
            synth->paramChanged(Synthesizer::SpeedParam, scaleSpeed(knobValue));
            break;

        default:
            return false;
    }

    return true;
}

void OscControlPanel::resized() {
    static const int space = 3;
    static const int topSpacer = 0;
    static const int iconSize = 24;

    int sliderHeight = getHeight() - 2 * space;
    int sliderWidth = (getWidth() - 4 * space) * 0.3333f + 0.5f;

    for (int i = 0; i < numSliders; ++i) {
        hSliders[i]->setBounds(sliderWidth * i + (i + 1) * space, space + topSpacer, sliderWidth, sliderHeight);
}
}

void OscControlPanel::writeXML(XmlElement* element) const {
    XmlElement* ctrlsElem = new XmlElement("OscControls");
    paramGroup->writeKnobXML(ctrlsElem);

    element->addChildElement(ctrlsElem);
}

bool OscControlPanel::readXML(const XmlElement* element) {
    XmlElement* ctrlsElem = element->getChildByName("OscControls");

    if (ctrlsElem == nullptr) {
        return false;
    }

    return paramGroup->readKnobXML(ctrlsElem);
}

void OscControlPanel::setLengthInSeconds(float value) {
    float unitValue = scaleSpeedInv(value);
    speed->setValue(unitValue, dontSendNotification);
}

void OscControlPanel::timerCallback() {
    volume->setCurrentValue(synth->getLastAudioLevel());
}

juce::Component* OscControlPanel::getComponent(int which) {
    switch (which) {
        case CycleTour::TargMasterVol:	return volume;	break;
        case CycleTour::TargMasterOct:	return octave;	break;
        case CycleTour::TargMasterLen:	return speed;	break;
        default:
            break;
    }

    return nullptr;
}

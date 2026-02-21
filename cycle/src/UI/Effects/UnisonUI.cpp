#include <App/EditWatcher.h>
#include <App/SingletonRepo.h>
#include <Obj/Ref.h>
#include <UI/Widgets/Knob.h>
#include <Util/ScopedBooleanSwitcher.h>
#include <Util/Util.h>

#include "UnisonUI.h"

#include <climits>
#include <Design/Updating/Updater.h>

#include "../../App/CycleTour.h"
#include "../../Audio/Effects/Unison.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Audio/Effects/AudioEffect.h"
#include "../../Util/CycleEnums.h"
#include "../CycleDefs.h"

UnisonUI::UnisonUI(SingletonRepo* repo, Effect* effect) :
        GuilessEffect("UnisonUI", "Unison", Unison::numParams, repo, effect, UpdateSources::SourceUnison)
    ,	voiceSelector	 (repo, this)
    ,	addRemover		 (this, repo, "Unison voice")
{
    Knob* orderKnob 	= new Knob(repo, Unison::Order,	"Unison Order", 0.0f);
    Knob* fineKnob 		= new Knob(repo, Unison::Fine, 	"Fine Tune", 	0.5f);
    Knob* detuneKnob 	= new Knob(repo, Unison::Width, "Detune Width", 0.5f);

    int maxOrder  = getConstant(MaxUnisonOrder);
    int maxDetune = getConstant(MaxDetune);

    using namespace Ops;
    StringFunction ordString(StringFunction(0).mul(maxOrder).add(1).max(maxOrder));
    StringFunction fineString(StringFunction(1).mul(2.f).sub(1.f));
    StringFunction detString(StringFunction(1).mul(maxDetune));

    detuneKnob->setStringFunctions(detString, detString.withPostString(" cents"));
    orderKnob->setStringFunctions(ordString, ordString.withPostString(" voice(s)"));
    fineKnob->setStringFunctions(fineString, fineString.clone().withPrecision(2));

    paramGroup->addSlider(detuneKnob);
    paramGroup->addSlider(new Knob(repo, Unison::PanSpread, "Pan Spread", 1.0f));
    paramGroup->addSlider(new Knob(repo, Unison::Phase, "Phase", 0.5f));
    paramGroup->addSlider(orderKnob);
    paramGroup->addSlider(new Knob(repo, Unison::Jitter, "Detune/Phase Jitter", 0.5f));
    paramGroup->addSlider(new Knob(repo, Unison::Pan, "Voice Pan", 0.5f));
    paramGroup->addSlider(fineKnob);

    minTitleSize = 70;

    modeBox.addItem("Group", Group);
    modeBox.addItem("Single", Single);
    modeBox.setSelectedId(Group, dontSendNotification);
    modeBox.setColour(ComboBox::outlineColourId, Colours::black);
    modeBox.addListener(this);
    modeBox.setWantsKeyboardFocus(false);

    voiceSelector.setItemName("Unison voice");
}

void UnisonUI::init() {
    GuilessEffect::init();

    unison = dynamic_cast<Unison*>(effect.get());
    jassert(unison != nullptr);
}

String UnisonUI::getKnobName(int index) const {
    bool little 	= getWidth() < 300;
    bool superSmall = getWidth() < 250;

    switch (index) {
        case Unison::Width: 	return superSmall ? "wth"	:  little ? 	"wdth" 	: "Width";
        case Unison::Order: 	return little ? 	"ord" 	: "Order";
        case Unison::PanSpread: return superSmall ? "pn" 	: "Pan";
        case Unison::Phase: 	return little ? 	"phs" 	: "Phase";
        case Unison::Jitter: 	return superSmall ? "jtr" 	: little ? "jttr" : "Jitter";
        case Unison::Fine: 		return superSmall ? "dtn%"	: little ? 	"dtn %" : "dtune %";
        case Unison::Pan: 		return "pan";
        default: throw std::out_of_range("UnisonUI::getKnobName");
    }
}

void UnisonUI::effectEnablementChanged(bool sendUIUpdate, bool sendDspUpdate) {
    if (sendDspUpdate) {
        unison->changeAllOrdersImplicit();
    }

    if (sendUIUpdate) {
        GuilessEffect::effectEnablementChanged(sendUIUpdate, sendDspUpdate);
    }
}

void UnisonUI::comboBoxChanged(ComboBox* box) {
    if (box == &modeBox) {
        getObj(EditWatcher).setHaveEditedWithoutUndo(true);
        modeChanged(true, true);
    }
}

void UnisonUI::modeChanged(bool updateAudio, bool graphicUpdate) {
    int id = modeBox.getSelectedId();

    bool falseIfGroupMode = id != Group;
    bool trueIfGroupMode = id == Group;

    addRemover.setVisible(falseIfGroupMode);
    voiceSelector.setVisible(falseIfGroupMode);

    paramGroup->getKnob<Slider>(Unison::Order)->setVisible(trueIfGroupMode);
    paramGroup->getKnob<Slider>(Unison::Jitter)->setVisible(trueIfGroupMode);
    paramGroup->getKnob<Slider>(Unison::PanSpread)->setVisible(trueIfGroupMode);
    paramGroup->getKnob<Knob>  (Unison::Width)->setColour(trueIfGroupMode ? Colour::greyLevel(0.65f) : Colour(0.11f, 0.5f, 0.95f, 1.f));
    paramGroup->getKnob<Slider>(Unison::Pan)->setVisible(falseIfGroupMode);
    paramGroup->getKnob<Slider>(Unison::Fine)->setVisible(falseIfGroupMode);

    Unison::ParamGroup& group = unison->getGraphicParams();
    Unison::UnivoiceData& data = group.voices[voiceSelector.currentIndex];

    // phase knob is reused
    paramGroup->getKnob<Slider>(Unison::Phase)->setValue(trueIfGroupMode ? group.phaseScale : data.phase, dontSendNotification);

    if (updateAudio) {
        unison->modeChanged();
    }

    if (graphicUpdate) {
        doUpdate(SourceUnison);
    }

    resized();
    repaint();
}

void UnisonUI::orderChangedTo(int order) {
    if (isGroupMode()) {
        double value = order / double(getConstant(MaxUnisonOrder) + 0.5f);
        paramGroup->getKnob<Slider>(Unison::Order)->setValue(value, dontSendNotification);
    } else {
        voiceSelector.numVoices = order;
        voiceSelector.currentIndex = jmin(voiceSelector.numVoices - 1,
                                          voiceSelector.currentIndex);

        voiceSelector.repaint();
    }
}

void UnisonUI::buttonClicked(Button* button) {
    if (button == &enableButton) {
        GuilessEffect::buttonClicked(button);
        return;
    }

    bool changed = true;

    if (button == &addRemover.add) {
        if (unison->addVoice(Unison::UnivoiceData(0.5f, 0.5f, 0.f))) {
            ++voiceSelector.numVoices;
            voiceSelector.currentIndex = voiceSelector.numVoices - 1;
        }
    } else if (button == &addRemover.remove) {
        if (voiceSelector.numVoices > 1) {
            if (unison->removeVoice(voiceSelector.currentIndex)) {
                --voiceSelector.numVoices;

                voiceSelector.currentIndex = jmin(voiceSelector.numVoices - 1,
                                                  voiceSelector.currentIndex);
            }
        } else {
            changed = false;
        }
    }

    if (changed) {
        voiceSelector.selectionChanged();

        if (enabled) {
            paramGroup->triggerRefreshUpdate();
        }
    }

    getObj(EditWatcher).setHaveEditedWithoutUndo(true);

    voiceSelector.repaint();
}

void UnisonUI::setExtraTitleElements(Rectangle<int>& right) {
    modeBox.setBounds(right.removeFromBottom(24));
}

void UnisonUI::setExtraRightElements(Rectangle<int>& r) {
    r.reduce(0, jmax(0, r.getHeight() - 36) / 2);

    if (!isGroupMode()) {
        r.removeFromRight(11);
        addRemover.setBounds(r.removeFromRight(24));

        r.removeFromRight(7);
        voiceSelector.setBounds(r.removeFromRight(24));

        r.removeFromRight(r.getWidth() < 150 ? -3 : -6);
    }
}

int UnisonUI::VoiceSelector::getSize() {
    return numVoices;
}

int UnisonUI::VoiceSelector::getCurrentIndexExternal() {
    return currentIndex;
}

void UnisonUI::VoiceSelector::selectionChanged() {
    Unison::ParamGroup& group = panel->unison->getGraphicParams();
    Unison::UnivoiceData& data = group.voices[currentIndex];

    panel->paramGroup->setKnobValue(Unison::Pan, data.pan, false);
    panel->paramGroup->setKnobValue(Unison::Phase, data.phase, false);
    panel->paramGroup->setKnobValue(Unison::Fine, data.finePct, false);
}

void UnisonUI::VoiceSelector::rowClicked(int row) {
    if (Util::assignAndWereDifferent(currentIndex, row)) {
        selectionChanged();
    }
}

void UnisonUI::writeXML(XmlElement* registryElem) const {
    auto* effectElem = new XmlElement(getEffectName());
    auto* knobsElem = new XmlElement("Knobs");

    for (int knobIdx = 0; knobIdx <= Unison::Jitter; ++knobIdx) {
        auto* knob = new XmlElement("Knob");

        knob->setAttribute("value", paramGroup->getKnob<Slider>(knobIdx)->getValue());
        knob->setAttribute("number", knobIdx);

        knobsElem->addChildElement(knob);
    }

    effectElem->addChildElement(knobsElem);

    auto* voiceDataElem = new XmlElement("VoiceData");
    const Unison::ParamGroup& group = unison->getGraphicParams();

    for (auto data : group.voices)
    {
        auto* dataElem = new XmlElement("data");

        dataElem->setAttribute("fine", 	data.finePct);
        dataElem->setAttribute("pan", 	data.pan);
        dataElem->setAttribute("phase", data.phase);

        voiceDataElem->addChildElement(dataElem);
    }

    effectElem->addChildElement(voiceDataElem);
    effectElem->setAttribute("mode", isGroupMode());
    effectElem->setAttribute("enabled", enabled);

    registryElem->addChildElement(effectElem);
}

void UnisonUI::updateSelection() {
    voiceSelector.selectionChanged();
}

bool UnisonUI::isGroupMode() const {
    return modeBox.getSelectedId() == Group;
}

bool UnisonUI::readXML(const XmlElement* element) {
    XmlElement* effectElem = element->getChildByName(getEffectName());

    if (effectElem != nullptr) {
        bool oldWasGroup = modeBox.getSelectedId() == Group;
        bool isGroup 	 = effectElem->getBoolAttribute("mode", true);

        unison->reset();

        setEffectEnabled(effectElem->getBoolAttribute("enabled"), false, true);

        // so that group knobs get set
        modeBox.setSelectedId(Group, dontSendNotification);
        paramGroup->readKnobXML(effectElem);

        modeBox.setSelectedId(isGroup ? Group : Single, dontSendNotification);

        XmlElement* voiceDataElem = effectElem->getChildByName("VoiceData");
        if (voiceDataElem != nullptr) {
            int count = 0;

            int limit = INT_MAX;

            vector<Unison::UnivoiceData> data;
            for (auto dataElem : voiceDataElem->getChildWithTagNameIterator("data")) {
                if (count++ >= limit) {
                    break;
}

                float finePct = dataElem->getDoubleAttribute("fine",	0.5);
                float pan 	  = dataElem->getDoubleAttribute("pan", 	0.5);
                float phase   = dataElem->getDoubleAttribute("phase", 	0.5);

                data.emplace_back(pan, finePct, phase);
            }

            if (!data.empty() && !isGroup) {
                unison->setVoices(data);
            }
        } else {
            // previous presets may have enlarged the voices array
            if (isGroup) {
                unison->trimVoicesToOrder();
            } else {
                unison->setVoiceData(0, 0.5, 0.5, 0.5);
            }
        }

        voiceSelector.currentIndex = 0;

        if (!isGroup) {
            voiceSelector.selectionChanged();
        }

        if (oldWasGroup != isGroup) {
            modeChanged(false, false);
        }

        repaint();
    } else if (isGroupMode()) {
        ScopedBooleanSwitcher sbs(paramGroup->updatingAllSliders);

        for (int i = 0; i <= Unison::Jitter; ++i) {
            paramGroup->setKnobValue(i, 0.5, false);
}
    }

    return true;
}

Array<Rectangle<int> > UnisonUI::getOutlinableRects() {
    Array<Rectangle<int> > a;

    a.add(enableButton.getBounds());

    if (!isGroupMode()) {
        a.add(voiceSelector.getBounds());
        a.add(addRemover.getBounds());
    }

    return a;
}

Array<int> UnisonUI::getApplicableKnobs() {
    Array<int> k;
    if (isGroupMode()) {
        int idx[] = { Unison::Width, Unison::PanSpread, Unison::Phase, Unison::Order, Unison::Jitter };

        k = Array<int>(idx, numElementsInArray(idx));
    } else {
        int idx[] = { Unison::Width, Unison::Fine, Unison::Pan, Unison::Phase };
        k = Array<int>(idx, numElementsInArray(idx));
    }

    return k;
}

juce::Component* UnisonUI::getComponent(int which) {
    if (which <= Unison::Fine) {
        return paramGroup->getKnob<Slider>(which);
}

    switch (which) {
        case CycleTour::TargUniVoiceSlct: return &voiceSelector;
        case CycleTour::TargUniMode:      return &modeBox;
        case CycleTour::TargUniAddRemove: return &addRemover;
        default: break;
    }

    return nullptr;
}

void UnisonUI::triggerModeChanged(bool isGroup) {
    modeBox.setSelectedId(isGroup ? Group : Single);
}

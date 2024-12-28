#include "ParameterGroup.h"

#include <Definitions.h>

#include "../App/EditWatcher.h"
#include "../App/Settings.h"
#include "../App/SingletonRepo.h"
#include "../Inter/UndoableActions.h"
#include "../Util/ScopedBooleanSwitcher.h"

ParameterGroup::ParameterGroup(SingletonRepo* repo, const String& name, Worker* worker) :
        SingletonAccessor   (repo, name)
    ,   worker              (worker)
    ,   updatingAllSliders  (false)
    ,   knobUIUpdater       (this)
    ,   sliderStartingValue (0.5)
{
}

bool ParameterGroup::paramTriggersAggregateUpdate(int knobIndex) {
    return !updatingAllSliders || (updatingAllSliders && knobIndex == knobs.size() - 1);
}

/*
 * User-interface source
 */
void ParameterGroup::sliderValueChanged(Slider* slider) {
    int knobIndex = knobs.indexOf(slider);
    if(knobIndex < 0) {
        return;
    }

    knobUIUpdater.knobValues[knobIndex] = slider->getValue();

    bool didAnythingSignificant = worker->updateDsp(knobIndex, slider->getValue(),
                                                    paramTriggersAggregateUpdate(knobIndex));

    if(didAnythingSignificant) {
        if(worker->shouldTriggerGlobalUpdate(slider) && getSetting(UpdateGfxRealtime)) {
            triggerRefreshUpdate();
        }

        else if(worker->shouldTriggerLocalUpdate(slider)) {
            worker->doLocalUIUpdate();
        }
    }
}

void ParameterGroup::sliderDragStarted(Slider* slider) {
    sliderStartingValue = slider->getValue();

    if(worker->shouldTriggerGlobalUpdate(slider) && getSetting(UpdateGfxRealtime)) {
        triggerReduceUpdate();
    }
}

void ParameterGroup::sliderDragEnded(Slider* slider) {
    auto* action = new SliderValueChangedAction(repo, worker->getUpdateSource(),
                                                slider, sliderStartingValue);
    getObj(EditWatcher).addAction(action, true);

    if(worker->shouldTriggerGlobalUpdate(slider)) {
        triggerRestoreUpdate();
    }
}

/*
 * Automation or persistence source
 */
void ParameterGroup::setKnobValue(int knobIndex, double knobValue,
                                  bool doGlobalUIUpdate, bool isAutomation) {
    jassert(knobIndex < knobs.size());

    bool didAnythingSignificant = worker->updateDsp(knobIndex, knobValue, paramTriggersAggregateUpdate(knobIndex));
    knobUIUpdater.setKnobUpdate(knobIndex, knobValue);

    if(isAutomation) {
        knobUIUpdater.triggerAsyncUpdate();
    } else {
        knobs[knobIndex]->setValue(knobValue, dontSendNotification);
    }

    if(doGlobalUIUpdate && didAnythingSignificant && worker->shouldTriggerGlobalUpdate(knobs[knobIndex])) {
        triggerRefreshUpdate();
    }
}

double ParameterGroup::getKnobValue(int knobIdx) const {
    return knobUIUpdater.knobValues[knobIdx];
}

bool ParameterGroup::readKnobXML(const XmlElement* effectElem) {
    XmlElement* knobsElem = effectElem->getChildByName("Knobs");

    if(knobsElem == nullptr) {
        return false;
    }

    ScopedBooleanSwitcher sbs(updatingAllSliders);

    for(auto currentKnob : knobsElem->getChildWithTagNameIterator("Knob")) {
        if(currentKnob == nullptr) {
            continue;
        }

        int number = currentKnob->getIntAttribute("number");
        if(number >= knobs.size()) {
            continue;
        }

        double value = currentKnob->getDoubleAttribute("value", knobs[number]->getValue());

        worker->overrideValueOptionally(number, value);
        setKnobValue(number, value, false);
    }

    worker->finishedUpdatingAllSliders();

    return true;
}

void ParameterGroup::writeKnobXML(XmlElement* effectElem) const {
    auto* knobsElem = new XmlElement("Knobs");

    for(int knobIdx = 0; knobIdx < knobs.size(); ++knobIdx) {
        auto* knob = new XmlElement("Knob");

        knob->setAttribute("value", knobs[knobIdx]->getValue());
        knob->setAttribute("number", knobIdx);
        knobsElem->addChildElement(knob);
    }

    effectElem->addChildElement(knobsElem);
}

void ParameterGroup::listenToKnobs() {
    for(int i = 0; i < knobs.size(); ++i) {
        Slider* knob = knobs.getUnchecked(i);
        knob->addListener(this);
        knob->setRange(0, 1);
        knobUIUpdater.knobValues[i] = knob->getValue();
    }
}

void ParameterGroup::addKnobsTo(Component* component) {
    for(auto slider : knobs) {
        component->addAndMakeVisible(slider);
    }
}

ParameterGroup::Worker::Worker(SingletonRepo* repo, const String& name) :
    paramGroup(new ParameterGroup(repo, name, this)) {
}

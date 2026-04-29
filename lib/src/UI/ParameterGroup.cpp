#include "ParameterGroup.h"

#include <Definitions.h>

#include "../App/Doc/PresetJson.h"
#include "../App/EditWatcher.h"
#include "../App/Settings.h"
#include "../App/SingletonRepo.h"
#include "../Inter/UndoableActions.h"
#include "../Util/ScopedBooleanSwitcher.h"

namespace {
    String describeSlider(Slider* slider) {
        if (slider == nullptr) {
            return "null";
        }

        String name = slider->getName();
        return name.isEmpty() ? "unnamed" : name;
    }
}

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

    bool aggregateUpdate = paramTriggersAggregateUpdate(knobIndex);
    DBG("ParameterGroup::sliderValueChanged"
        + String(" group=") + getName()
        + " knob=" + String(knobIndex)
        + " slider=" + describeSlider(slider)
        + " value=" + String(slider->getValue(), 6)
        + " aggregate=" + String(aggregateUpdate ? 1 : 0)
        + " realtime=" + String(getSetting(UpdateGfxRealtime) ? 1 : 0));

    bool didAnythingSignificant = worker->updateDsp(knobIndex, slider->getValue(), aggregateUpdate);
    bool globalUpdate = worker->shouldTriggerGlobalUpdate(slider);
    bool localUpdate  = worker->shouldTriggerLocalUpdate(slider);

    DBG("ParameterGroup::sliderValueChanged result"
        + String(" group=") + getName()
        + " knob=" + String(knobIndex)
        + " changed=" + String(didAnythingSignificant ? 1 : 0)
        + " global=" + String(globalUpdate ? 1 : 0)
        + " local=" + String(localUpdate ? 1 : 0));

    if(didAnythingSignificant) {
        if(globalUpdate && getSetting(UpdateGfxRealtime)) {
            DBG("ParameterGroup::sliderValueChanged triggerRefreshUpdate group=" + getName());
            triggerRefreshUpdate();
        }

        else if(localUpdate) {
            DBG("ParameterGroup::sliderValueChanged doLocalUIUpdate group=" + getName());
            worker->doLocalUIUpdate();
        }
    }
}

void ParameterGroup::sliderDragStarted(Slider* slider) {
    sliderStartingValue = slider->getValue();
    bool globalUpdate = worker->shouldTriggerGlobalUpdate(slider);

    DBG("ParameterGroup::sliderDragStarted"
        + String(" group=") + getName()
        + " slider=" + describeSlider(slider)
        + " value=" + String(sliderStartingValue, 6)
        + " global=" + String(globalUpdate ? 1 : 0)
        + " realtime=" + String(getSetting(UpdateGfxRealtime) ? 1 : 0));

    if(globalUpdate && getSetting(UpdateGfxRealtime)) {
        triggerReduceUpdate();
    }
}

void ParameterGroup::sliderDragEnded(Slider* slider) {
    auto* action = new SliderValueChangedAction(repo, worker->getUpdateSource(),
                                                slider, sliderStartingValue);
    getObj(EditWatcher).addAction(action, true);

    bool globalUpdate = worker->shouldTriggerGlobalUpdate(slider);
    DBG("ParameterGroup::sliderDragEnded"
        + String(" group=") + getName()
        + " slider=" + describeSlider(slider)
        + " start=" + String(sliderStartingValue, 6)
        + " end=" + String(slider->getValue(), 6)
        + " global=" + String(globalUpdate ? 1 : 0));

    if(globalUpdate) {
        triggerRestoreUpdate();
    }
}

void ParameterGroup::doGlobalUIUpdate(bool force) {
    DBG("ParameterGroup::doGlobalUIUpdate"
        + String(" group=") + getName()
        + " force=" + String(force ? 1 : 0));

    worker->doGlobalUIUpdate(force);
}

void ParameterGroup::reduceDetail() {
    DBG("ParameterGroup::reduceDetail group=" + getName());
    worker->reduceDetail();
}

void ParameterGroup::restoreDetail() {
    DBG("ParameterGroup::restoreDetail group=" + getName());
    worker->restoreDetail();
}

/*
 * Automation or persistence source
 */
void ParameterGroup::setKnobValue(int knobIndex, double knobValue,
                                  bool doGlobalUIUpdate, bool isAutomation) {
    jassert(knobIndex < knobs.size());

    bool aggregateUpdate = paramTriggersAggregateUpdate(knobIndex);
    DBG("ParameterGroup::setKnobValue"
        + String(" group=") + getName()
        + " knob=" + String(knobIndex)
        + " value=" + String(knobValue, 6)
        + " aggregate=" + String(aggregateUpdate ? 1 : 0)
        + " doGlobal=" + String(doGlobalUIUpdate ? 1 : 0)
        + " automation=" + String(isAutomation ? 1 : 0));

    bool didAnythingSignificant = worker->updateDsp(knobIndex, knobValue, aggregateUpdate);
    knobUIUpdater.setKnobUpdate(knobIndex, knobValue);

    if(isAutomation) {
        knobUIUpdater.triggerAsyncUpdate();
    } else {
        knobs[knobIndex]->setValue(knobValue, dontSendNotification);
    }

    bool globalUpdate = worker->shouldTriggerGlobalUpdate(knobs[knobIndex]);
    DBG("ParameterGroup::setKnobValue result"
        + String(" group=") + getName()
        + " knob=" + String(knobIndex)
        + " changed=" + String(didAnythingSignificant ? 1 : 0)
        + " global=" + String(globalUpdate ? 1 : 0));

    if(doGlobalUIUpdate && didAnythingSignificant && globalUpdate) {
        DBG("ParameterGroup::setKnobValue triggerRefreshUpdate group=" + getName());
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

bool ParameterGroup::readKnobJSON(const var& json) {
    const Array<var>* knobValues = PresetJson::getArray(json);

    if (knobValues == nullptr) {
        return false;
    }

    ScopedBooleanSwitcher sbs(updatingAllSliders);

    for (int i = 0; i < jmin(knobs.size(), knobValues->size()); ++i) {
        const var& value = knobValues->getReference(i);
        double knobValue = value.isObject()
                         ? PresetJson::doubleProperty(value, "value", knobs[i]->getValue())
                         : double(value);

        worker->overrideValueOptionally(i, knobValue);
        setKnobValue(i, knobValue, false);
    }

    worker->finishedUpdatingAllSliders();

    return true;
}

var ParameterGroup::writeKnobJSON() const {
    Array<var> values;

    for (int knobIdx = 0; knobIdx < knobs.size(); ++knobIdx) {
        values.add(knobs[knobIdx]->getValue());
    }

    return var(values);
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

#include <App/SingletonRepo.h>
#include <App/Doc/PresetJson.h>
#include "EffectGuiRegistry.h"
#include "ReverbUI.h"
#include "DelayUI.h"
#include "IrModellerUI.h"
#include "WaveshaperUI.h"
#include "UnisonUI.h"
#include "EqualizerUI.h"

EffectGuiRegistry::EffectGuiRegistry(SingletonRepo* repo) : SingletonAccessor(repo, "EffectGuiRegistry") {
}

bool EffectGuiRegistry::readXML(const XmlElement* topElement) {
    XmlElement* effectsElem = topElement->getChildByName("Effects");

    if (!effectsElem)
        return false;

    Savable* effects[] = {
        &getObj(IrModellerUI),
        &getObj(UnisonUI),
        &getObj(WaveshaperUI),
        &getObj(DelayUI),
        &getObj(ReverbUI),
        &getObj(EqualizerUI)
    };

    for (auto& effect : effects) {
        if (!effect->readXML(effectsElem)) {
            return false;
        }
    }

    return true;
}

void EffectGuiRegistry::writeXML(XmlElement* topElement) const {
    auto* effectsElem = new XmlElement("Effects");

    const Savable* effects[] = {
        &getObj(IrModellerUI),
        &getObj(UnisonUI),
        &getObj(WaveshaperUI),
        &getObj(DelayUI),
        &getObj(ReverbUI),
        &getObj(EqualizerUI)
    };

    for (auto& effect : effects) {
        effect->writeXML(effectsElem);
    }

    topElement->addChildElement(effectsElem);
}

var EffectGuiRegistry::writeJSON() const {
    auto json = PresetJson::object();

    json->setProperty("ImpulseModeller", getObj(IrModellerUI).writeJSON());
    json->setProperty("Unison", getObj(UnisonUI).writeJSON());
    json->setProperty("Waveshaper", getObj(WaveshaperUI).writeJSON());
    json->setProperty("Delay", getObj(DelayUI).writeJSON());
    json->setProperty("Reverb", getObj(ReverbUI).writeJSON());
    json->setProperty("EQ", getObj(EqualizerUI).writeJSON());

    return PresetJson::toVar(json);
}

bool EffectGuiRegistry::readJSON(const var& object) {
    bool success = true;

    success &= getObj(IrModellerUI).readJSON(PresetJson::property(object, "ImpulseModeller"));
    success &= getObj(UnisonUI).readJSON(PresetJson::property(object, "Unison"));
    success &= getObj(WaveshaperUI).readJSON(PresetJson::property(object, "Waveshaper"));
    success &= getObj(DelayUI).readJSON(PresetJson::property(object, "Delay"));
    success &= getObj(ReverbUI).readJSON(PresetJson::property(object, "Reverb"));
    success &= getObj(EqualizerUI).readJSON(PresetJson::property(object, "EQ"));

    return success;
}

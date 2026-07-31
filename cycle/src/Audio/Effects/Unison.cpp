#include <Obj/Ref.h>
#include <Audio/CycleDsp/UnisonCore.h>

#include "JuceHeader.h"
#include "Unison.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../UI/Effects/UnisonUI.h"
#include "../../Util/CycleEnums.h"

Unison::Unison(SingletonRepo* repo) : Effect(repo, "Unison")
                                      , removeVoiceAction(RemoveVoice)
                                      , addVoiceAction(AddVoice)
                                      , changeOrderAction(ChangeOrder)
                                      , updateAllAction(UpdateAll)
                                      , setVoicesAction(SetVoices)
                                      , groupMode(true) {
    actions.add(&removeVoiceAction);
    actions.add(&addVoiceAction);
    actions.add(&changeOrderAction);
    actions.add(&updateAllAction);
    actions.add(&setVoicesAction);

    ParamGroup* groups[] = { &audioParams, &graphicParams };

    for (auto & j : groups) {
        ParamGroup& group = *j;

        group.voices.emplace_back();
        group.groupModeOrder = 1;
    }

}

void Unison::setUI(UnisonUI* comp) {
    ui = comp;

    for (int i = 0; i < 2; ++i) {
        bool isAudioThread = i == 0;

        updatePanning(isAudioThread);
        updatePhases(isAudioThread);
        updateTunings(isAudioThread);
    }
}

// dummy function, not to be used
void Unison::processBuffer(AudioSampleBuffer& buffer) {
}

int Unison::getOrder(bool isAudio) {
    if (!isEnabled()) {
        return 1;
    }

    ParamGroup& group = isAudio ? audioParams : graphicParams;
    return isGroupMode() ? group.groupModeOrder : group.voices.size();
}

void Unison::updateTunings(bool isAudio, bool force) {
    ParamGroup& group = isAudio ? audioParams : graphicParams;
    int order = getOrder(isAudio);

    jassert(group.voices.size() >= order);

    if (isGroupMode() || force) {
        CycleDsp::UnisonGroupConfiguration configuration;
        configuration.order = order;
        configuration.detuneWidthCents = group.width;
        configuration.jitter = group.jitterScale;
        const auto layout = CycleDsp::UnisonCore::makeGroupLayout(configuration);
        for (int i = 0; i < layout.order; ++i) {
            group.voices[i].finePct = layout[i].detunePosition;
            group.voices[i].fine = layout[i].detuneCents;
        }
    } else {
        for (int i = 0; i < order; ++i) {
            group.voices[i].fine = CycleDsp::UnisonCore::detuneCentsFromPosition(
                    group.voices[i].finePct,
                    group.width);
        }
    }
}

void Unison::updatePanning(bool isAudio, bool force) {
    ParamGroup& group = isAudio ? audioParams : graphicParams;
    int order = getOrder(isAudio);

    jassert(group.voices.size() >= order);

    if (isGroupMode() || force) {
        CycleDsp::UnisonGroupConfiguration configuration;
        configuration.order = order;
        configuration.panSpread = group.panScale;
        const auto layout = CycleDsp::UnisonCore::makeGroupLayout(configuration);
        for (int i = 0; i < layout.order; ++i) {
            group.voices[i].pan = layout[i].pan;
        }
    }
}

void Unison::updatePhases(bool isAudio, bool force) {
    ParamGroup& group = isAudio ? audioParams : graphicParams;
    int order = getOrder(isAudio);

    jassert(group.voices.size() >= order);

    if (isGroupMode() || force) {
        CycleDsp::UnisonGroupConfiguration configuration;
        configuration.order = order;
        configuration.phaseSpread = group.phaseScale;
        configuration.jitter = group.jitterScale;
        const auto layout = CycleDsp::UnisonCore::makeGroupLayout(configuration);
        for (int i = 0; i < layout.order; ++i) {
            group.voices[i].phase = layout[i].phaseCycles;
        }
    }
}

int Unison::calcOrder(double value) {
    return CycleDsp::UnisonCore::orderFromUnitValue(value);
}

bool Unison::isEnabled() const {
    return ui->isEffectEnabled();
}

bool Unison::isGroupMode() const {
    return groupMode;
}

double Unison::getDetune(int unisonIndex, bool isAudio) {
    ParamGroup& group = isAudio ? audioParams : graphicParams;

    float& fine = group.voices[unisonIndex].fine;

    return isEnabled() ? fine : 0.f;
}

double Unison::getPhase(int unisonIndex, bool isAudio) {
    ParamGroup& group = isAudio ? audioParams : graphicParams;

    return isEnabled() ? group.voices[unisonIndex].phase : 0.f;
}

double Unison::getPan(int unisonIndex, bool isAudio) {
    ParamGroup& group = isAudio ? audioParams : graphicParams;

    return isEnabled() ? group.voices[unisonIndex].pan : 0.5f;
}

bool Unison::isStereo() {
    if (!isGroupMode()) {
        if (audioParams.voices.size() == 1) {
            return false;
        }

        for (auto& voice: audioParams.voices) {
            if (voice.pan != 0.5f) {
                return true;
            }
        }

        return false;
    }

    return audioParams.groupModeOrder > 1 &&
           audioParams.panScale != 0.;
}

bool Unison::isPhased() {
    if (!isGroupMode()) {
        if (audioParams.voices.size() == 1) {
            return false;
        }

        for (auto& voice: audioParams.voices) {
            if (voice.phase != 0.f) {
                return true;
            }
        }

        return false;
    }

    return audioParams.phaseScale != 0;
}

void Unison::audioThreadUpdate() {
    for (auto action: actions) {
        if (!action->isPending()) {
            continue;
        }

        switch (action->getId()) {
            case RemoveVoice: {
                int idx = removeVoiceAction.getValue();

                if (idx < audioParams.voices.size()) {
                    audioParams.voices.erase(audioParams.voices.begin() + idx);
                }
                break;
            }

            case AddVoice:
                audioParams.voices.push_back(addVoiceAction.getValue());
                break;

            case ChangeOrder: {
                int order = changeOrderAction.getValue();

                audioParams.groupModeOrder = order;

                increaseVoicesToGroupOrder(true);

                if (isGroupMode()) {
                    updateAll(true);
                }

                break;
            }

            case UpdateAll: {
                bool force = updateAllAction.getValue();

                updateAll(true, force);
                break;
            }

            case SetVoices:
                audioParams.voices = setVoicesAction.getValue();
                updateTunings(true);
                break;

            default:
                throw std::runtime_error("Unison::audioThreadUpdate: Invalid action id");
        }

        action->dismiss();
    }
}

bool Unison::doParamChange(int index, double value, bool doFurtherUpdate) {
    if (isGroupMode()) {
        if (index == Order) {
            changeOrderFromValue(false, value);

            return changeOrderFromValue(true, value);
        }

        ParamGroup* groups[] = { &audioParams, &graphicParams };
        for (int i = 0; i < 2; ++i) {
            ParamGroup& group = *groups[i];

            switch (index) {
                case Width: {
                    double detune = value * getConstant(MaxDetune);

                    group.width = detune;
                    updateTunings(i == 0);
                    break;
                }

                case PanSpread:
                    group.panScale = value;
                    updatePanning(i == 0);
                    break;

                case Phase:
                    group.phaseScale = value;
                    updatePhases(i == 0);
                    break;

                case Jitter:
                    group.jitterScale = value;
                    updateTunings(i == 0);
                    updatePhases(i == 0);
                    break;

                default:
                    break;
            }
        }
    } else {
        int uniIndex = ui->getCurrentIndex();

        ParamGroup* groups[] = { &audioParams, &graphicParams };

        jassert(! addVoiceAction.isPending());
        jassert(! setVoicesAction.isPending());

        for (int i = 0; i < 2; ++i) {
            ParamGroup& group = *groups[i];
            UnivoiceData& data = group.voices[uniIndex];

            switch (index) {
                case Width: {
                    float detune = value * getConstant(MaxDetune);

                    group.width = detune;
                    updateTunings(i == 0);
                    break;
                }

                case Fine: {
                    data.finePct = value;
                    data.fine = CycleDsp::UnisonCore::detuneCentsFromPosition(
                            value,
                            group.width);
                    break;
                }

                case Phase: {
                    data.phase = value;
                    break;
                }

                case Pan: {
                    data.pan = value;
                    break;
                }

                default:
                    break;
            }
        }
    }

    return true;
}

void Unison::changeAllOrdersImplicit() {
    changeOrderTo(true, -1);
    changeOrderTo(false, -1);
}

double Unison::getUIKnobValue(int param) {
    if (ui == nullptr) {
        return 0.5;
    }

    if (auto* slider = ui->getParamGroup().getKnob<Slider>(param)) {
        return slider->getValue();
    }

    return ui->getParamGroup().getKnobValue(param);
}

void Unison::syncParamsFromUI() {
    if (isGroupMode()) {
        ParamGroup* groups[] = { &audioParams, &graphicParams };

        for (int i = 0; i < 2; ++i) {
            ParamGroup& group = *groups[i];
            group.width       = getUIKnobValue(Width) * getConstant(MaxDetune);
            group.panScale    = getUIKnobValue(PanSpread);
            group.phaseScale  = getUIKnobValue(Phase);
            group.jitterScale = getUIKnobValue(Jitter);
        }

        int newOrder = isEnabled() ? calcOrder(getUIKnobValue(Order)) : 1;
        changeOrderTo(true, newOrder);
        changeOrderTo(false, newOrder);
        updateAll(false, true);
        return;
    }

    int uniIndex = ui->getCurrentIndex();

    if (!isPositiveAndBelow(uniIndex, (int) graphicParams.voices.size())) {
        return;
    }

    ParamGroup* groups[] = { &audioParams, &graphicParams };
    for (auto* group : groups) {
        group->width = getUIKnobValue(Width) * getConstant(MaxDetune);

        while (!isPositiveAndBelow(uniIndex, (int) group->voices.size())) {
            group->voices.emplace_back();
        }

        UnivoiceData& data = group->voices[uniIndex];
        data.finePct = getUIKnobValue(Fine);
        data.fine    = CycleDsp::UnisonCore::detuneCentsFromPosition(
                data.finePct,
                group->width);
        data.pan     = getUIKnobValue(Pan);
        data.phase   = getUIKnobValue(Phase);
    }
}

bool Unison::changeOrderFromValue(bool isAudio, double orderValue) {
    if (orderValue < 0) {
        orderValue = getUIKnobValue(Order);
    }

    int newOrder = isEnabled() ? calcOrder(orderValue) : 1;

    return changeOrderTo(isAudio, newOrder);
}

bool Unison::changeOrderTo(bool isAudio, int newOrder) {
    if (newOrder < 0) {
        double orderValue = getUIKnobValue(Order);
        newOrder = isEnabled() ? calcOrder(orderValue) : 1;
    }
    ParamGroup& group = isAudio ? audioParams : graphicParams;
    int oldOrder = getOrder(isAudio); // group.groupModeOrder;

    if (isAudio) {
        changeOrderAction.setValueAndTrigger(newOrder);
    } else {
        group.groupModeOrder = newOrder;

        // audio voice values will be updated at note start
        updateAll(false);
        getObj(SynthAudioSource).unisonOrderChanged();
    }

    return oldOrder != newOrder;
}

void Unison::setGroupMode(bool isGroupMode) {
    groupMode = isGroupMode;
}

void Unison::updateAll(bool isAudio, bool force) {
    increaseVoicesToGroupOrder(isAudio);
    updateTunings(isAudio, force);
    updatePanning(isAudio, force);
    updatePhases(isAudio, force);
}

void Unison::increaseVoicesToGroupOrder(bool isAudio) {
    ParamGroup& group = isAudio ? audioParams : graphicParams;

    while ((int) group.voices.size() < group.groupModeOrder) {
        group.voices.emplace_back();
    }
}

void Unison::setVoiceData(int unisonIdx, float fine, float pan, float phase) {
    //	dout << "Setting unison voice data: " << unisonIdx << ", " << fine << ", " << pan << ", " << phase << "\n";

    ParamGroup* groups[] = { &audioParams, &graphicParams };

    for (auto& i : groups) {
        ParamGroup& group = *i;

        while (group.voices.size() <= unisonIdx) {
            group.voices.emplace_back();
        }

        UnivoiceData& data = group.voices[unisonIdx];
        data = UnivoiceData(pan, fine, phase);
        data.fine = CycleDsp::UnisonCore::detuneCentsFromPosition(
                data.finePct,
                group.width);

        if (!isGroupMode()) {
            group.groupModeOrder = group.voices.size();
        }
    }
}

bool Unison::removeVoice(int unisonIdx) {
    if (removeVoiceAction.isPending()) {
        return false;
    }

    if (unisonIdx < graphicParams.voices.size())
        graphicParams.voices.erase(graphicParams.voices.begin() + unisonIdx);

    removeVoiceAction.setValueAndTrigger(unisonIdx);
    getObj(SynthAudioSource).unisonOrderChanged();

    return true;
}

bool Unison::addVoice(const UnivoiceData& data, bool async) {
    if (async) {
        if (addVoiceAction.isPending())
            return false;

        graphicParams.voices.push_back(data);
        addVoiceAction.setValueAndTrigger(data);
    } else {
        graphicParams.voices.push_back(data);
        audioParams.voices.push_back(data);
    }

    getObj(SynthAudioSource).unisonOrderChanged();

    return true;
}

void Unison::modeChanged() {
    int numVoices = 1;

    if (!isEnabled()) {
        return;
    }

    numVoices = isGroupMode()
                    ? jmin<int>(maxUnisonOrder, (int) graphicParams.voices.size())
                    : jmax<int>((int) graphicParams.voices.size(),
                                calcOrder(ui->getParamGroup().getKnobValue(Unison::Order)));

    changeOrderTo(true, numVoices);
    changeOrderTo(false, numVoices);

    ui->orderChangedTo(numVoices);

    if (!isGroupMode()) {
        updateAll(false, true);
        ui->updateSelection();
    }
}

void Unison::setVoices(vector<UnivoiceData>& data) {
    jassert(! isGroupMode());

    graphicParams.voices = data;
    updateTunings(false);

    setVoicesAction.setValueAndTrigger(data);
    ui->orderChangedTo(data.size());
    getObj(SynthAudioSource).unisonOrderChanged();
}

void Unison::reset() {
    ParamGroup* groups[] = { &audioParams, &graphicParams };

    for (auto& i : groups) {
        ParamGroup& group = *i;
        group.voices.clear();
        group.voices.emplace_back();

        group.groupModeOrder = 1;
        group.jitterScale = 0;
        group.panScale = 0;
        group.phaseScale = 0;
        group.width = getConstant(MaxDetune) * 0.5f;
    }
}

void Unison::trimVoicesToOrder() {
    ParamGroup* groups[] = { &audioParams, &graphicParams };

    for (auto& i : groups) {
        ParamGroup& group = *i;

        while ((int) group.voices.size() > group.groupModeOrder)
            group.voices.pop_back();
    }
}

#include <Obj/Ref.h>
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
                                      , setVoicesAction(SetVoices) {
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

    double tempJitters[maxUnisonOrder - 1][maxUnisonOrder] =
    {
        {  0.001, -0.001 },
        { -0.108, -0.061,  0.163 },
        {  0.023, -0.067, -0.124,  0.168 },
        { -0.135,  0.094,  0.022, -0.122,  0.143 },
        { -0.076, -0.111,  0.123, -0.093,  0.178, -0.062 },
        {  0.061, -0.066,  0.056, -0.083,  0.215,  0.123, -0.018 },
        { -0.013, -0.061,  0.115,  0.231, -0.024, -0.172,  0.156,  0.155 },
        {  0.023, -0.065, -0.128,  0.169, -0.127,  0.149, -0.103, -0.062,  0.163 },
        { -0.068,  0.054, -0.086,  0.212,  0.126, -0.014,  0.023, -0.063, -0.128, 0.163 },
    };

    for (int i = 0; i < maxUnisonOrder - 1; ++i) {
        for (int j = 0; j < i + 2; ++j) {
            jitters[i][j] = tempJitters[i][j];
        }
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
    return ui->isGroupMode() ? group.groupModeOrder : group.voices.size();
}

void Unison::updateTunings(bool isAudio, bool force) {
    ParamGroup& group = isAudio ? audioParams : graphicParams;
    int order = getOrder(isAudio);

    jassert(group.voices.size() >= order);

    if (ui->isGroupMode() || force) {
        if (order == 1) {
            group.voices.front().fine = 0.;
        } else {
            int minOrder = jmin<int>(maxUnisonOrder, order);
            float increment = 1 / float(minOrder - 1);
            int jitterIdx = minOrder - 2;

            // don't touch the single mode voices above size of jitter array
            for (int i = 0; i < minOrder; ++i) {
                UnivoiceData& data = group.voices[i];

                data.finePct = ((2 * increment * i - 1) - group.jitterScale * jitters[jitterIdx][i]);
                data.finePct = 0.5f * (data.finePct + 1.f);
                data.fine = group.width * (data.finePct * 2 - 1.f);
            }
        }
    } else {
        for (int i = 0; i < order; ++i) {
            group.voices[i].fine = (2 * group.voices[i].finePct - 1) * group.width;
        }
    }
}

void Unison::updatePanning(bool isAudio, bool force) {
    ParamGroup& group = isAudio ? audioParams : graphicParams;
    int order = getOrder(isAudio);

    jassert(group.voices.size() >= order);

    if (ui->isGroupMode() || force) {
        if (order == 1) {
            group.voices.front().pan = 0.5;
            return;
        }

        if (order % 2 == 1) {
            int i = 0;
            for (; i < order / 2; ++i)
                group.voices[i].pan = 0.5f + group.panScale * 0.5f * cos(IPP_PI * i);

            for (; i < order; ++i)
                group.voices[i].pan = 0.5f - group.panScale * 0.5f * cos(IPP_PI * i);

            group.voices[order / 2].pan = 0.5;
        } else {
            for (int i = 0; i < order; ++i)
                group.voices[i].pan = 0.5f + group.panScale * 0.5f * cos(IPP_PI * i);
        }
    }
}

void Unison::updatePhases(bool isAudio, bool force) {
    ParamGroup& group = isAudio ? audioParams : graphicParams;
    int order = getOrder(isAudio);

    jassert(group.voices.size() >= order);

    if (ui->isGroupMode() || force) {
        if (order == 1) {
            group.voices.front().phase = 0.;
        } else {
            int minOrder = jmin<int>(maxUnisonOrder, order);
            float increment = 1 / float(minOrder);
            int jitterIdx = minOrder - 2;

            for (int i = 0; i < jmin<int>(order, maxUnisonOrder); ++i) {
                float phase = group.phaseScale * ((double(i) - (order - 1) * 0.5) * increment +
                                                  group.jitterScale * jitters[jitterIdx][i]);
                if (phase < 0) {
                    phase += 1;
                }

                group.voices[i].phase = phase;
            }
        }
    }
}

int Unison::calcOrder(double value) {
    return jmin<int>(maxUnisonOrder, int(maxUnisonOrder * value + 1.));
}

bool Unison::isEnabled() const {
    return ui->isEffectEnabled();
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
    if (!ui->isGroupMode()) {
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
    if (!ui->isGroupMode()) {
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

                if (ui->isGroupMode()) {
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
    if (ui->isGroupMode()) {
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
                    data.fine = group.width * (2.f * value - 1);
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
    ParamGroup* groups[] = { &audioParams, &graphicParams };

    changeOrderTo(true, -1);
    changeOrderTo(false, -1);
}

bool Unison::changeOrderFromValue(bool isAudio, double orderValue) {
    if (orderValue < 0) {
        orderValue = ui->getParamGroup().getKnobValue(Order);
    }

    int newOrder = isEnabled() ? calcOrder(orderValue) : 1;

    return changeOrderTo(isAudio, newOrder);
}

bool Unison::changeOrderTo(bool isAudio, int newOrder) {
    if (newOrder < 0) {
        double orderValue = ui->getParamGroup().getKnobValue(Order);
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
        data.fine = group.width * (2.f * data.finePct - 1.f);

        if (!ui->isGroupMode()) {
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

    numVoices = ui->isGroupMode()
                    ? jmin<int>(maxUnisonOrder, (int) graphicParams.voices.size())
                    : jmax<int>((int) graphicParams.voices.size(),
                                calcOrder(ui->getParamGroup().getKnobValue(Unison::Order)));

    changeOrderTo(true, numVoices);
    changeOrderTo(false, numVoices);

    ui->orderChangedTo(numVoices);

    if (!ui->isGroupMode()) {
        updateAll(false, true);
        ui->updateSelection();
    }
}

void Unison::setVoices(vector<UnivoiceData>& data) {
    jassert(! ui->isGroupMode());

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

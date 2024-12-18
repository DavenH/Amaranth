#pragma once

#include <vector>
#include <Thread/PendingAction.h>
#include "AudioEffect.h"

using std::vector;

class UnisonUI;

class Unison :
    public Effect
{
public:
    enum { maxUnisonOrder = 10 };

    struct UnivoiceData
    {
        UnivoiceData()
                : pan(0), fine(0), phase(0), finePct(0.5)
        {
        }

        UnivoiceData(float pan, float finePct, float phase)
                : pan(pan), phase(phase), finePct(finePct), fine(0)
        {
        }

        float pan;
        float finePct;
        float fine;
        float phase;
    };

    // each voice has it's own parameter group that is gauranteed to
    // be consistent from beginning to end of the calculation phases of
    // the voice
    class ParamGroup
    {
    public:
        ParamGroup()
                : panScale(0.5f), phaseScale(0.5f), jitterScale(0.8f), width(0.f), groupModeOrder(1)
        {
        }

        vector<UnivoiceData> voices;
        float panScale;
        float phaseScale;
        float jitterScale;
        float width;
        int groupModeOrder;
//		int destOrder;
    };

    explicit Unison(SingletonRepo* repo);

    enum Param { Width, PanSpread, Phase, Order, Jitter, Pan, Fine,
                 numParams };

    // dummy function, not to be used
    void processBuffer(AudioSampleBuffer& buffer) override;

    void audioThreadUpdate() override;
    void updateTunings(bool isAudio, bool force = false);
    void updatePanning(bool isAudio, bool force = false);
    void updatePhases(bool isAudio, bool force = false);
    void updateAll(bool isAudio, bool force = false);
    void increaseVoicesToGroupOrder(bool isAudio);

    int getOrder(bool isAudio);
    double getPan(int unisonIndex, bool isAudio = true);
    double getPhase(int unisonIndex, bool isAudio = true);
    double getDetune(int unisonIndex, bool isAudio = true);

    void setUI(UnisonUI* comp);
    void setVoiceData(int voiceIndex, float fine, float pan, float phase);
    void setVoices(vector<UnivoiceData>& data);
    void modeChanged();
    void reset() override;
    bool removeVoice(int unisonIdx);
    bool addVoice(const UnivoiceData& data, bool async = true);

    bool isStereo();
    bool isPhased();
    bool isEnabled() const override;
    bool changeOrderFromValue(bool isAudio, double orderValue);
    bool changeOrderTo(bool isAudio, int newOrder);

    void changeAllOrdersImplicit();
    void trimVoicesToOrder();

    [[nodiscard]] const ParamGroup& getGraphicParams() const	{ return graphicParams; }
    ParamGroup& getGraphicParams() 				{ return graphicParams; }

    static int calcOrder(double value);

    enum
    {
            RemoveVoice
        ,	AddVoice
        ,	ChangeOrder
        ,	UpdateAll
        ,	SetVoices
    };
protected:
    bool doParamChange(int index, double value, bool doFutherUpdate) override;

private:
    PendingActionValue<bool> updateAllAction;
    PendingActionValue<int> changeOrderAction;
    PendingActionValue<int> removeVoiceAction;
    PendingActionValue<UnivoiceData> addVoiceAction;
    PendingActionValue<vector<UnivoiceData> > setVoicesAction;

    Ref<UnisonUI> ui;
    Array<PendingAction*> actions;

    ParamGroup graphicParams;
    ParamGroup audioParams;

    double jitters[maxUnisonOrder - 1][maxUnisonOrder];
};
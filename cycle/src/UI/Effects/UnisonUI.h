#pragma once

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>

#include "GuilessEffect.h"
#include "../Widgets/Controls/LayerAddRemover.h"
#include "../Widgets/Controls/SelectorPanel.h"

class Unison;
class Effect;

class UnisonUI :
        public GuilessEffect
    ,	public ComboBox::Listener
{
public:
    class VoiceSelector : public SelectorPanel
    {
    public:
        VoiceSelector(SingletonRepo* repo, UnisonUI* panel) :
                SelectorPanel(repo)
            ,	panel		(panel)
            ,	numVoices	(1)
        {}

        int getSize() override;
        int getCurrentIndexExternal() override;
        void selectionChanged() override;
        void rowClicked(int row) override;

        Ref<UnisonUI> panel;

        int numVoices;
    };

    UnisonUI(SingletonRepo* repo, Effect* effect);

    ~UnisonUI() override = default;

    Array<Rectangle<int> > getOutlinableRects() override;
    Array<int> getApplicableKnobs() override;
    [[nodiscard]] bool isGroupMode() const;
    int getCurrentIndex() { return voiceSelector.getCurrentIndex(); }
    void modeChanged(bool updateAudio, bool graphicUpdate);
    void orderChangedTo(int order);
    void updateSelection();

    void setExtraTitleElements(Rectangle<int>& r) override;
    void setExtraRightElements(Rectangle<int>& r) override;

    void effectEnablementChanged(bool sendUIUpdate, bool sendDspUpdate) override;
    void comboBoxChanged (ComboBox* box) override;
    void buttonClicked(Button* button) override;
    void triggerModeChanged(bool isGroup);

    [[nodiscard]] String getKnobName(int index) const override;
    Component* getComponent(int which) override;

    void init() override;
    void writeXML(XmlElement* registryElem) const override;
    bool readXML(const XmlElement* element) override;

    enum UniMode
    {
        Group = 1, Single
    };

    friend class VoiceSelector;

private:
    Unison* unison;
    ComboBox modeBox;
    VoiceSelector voiceSelector;
    LayerAddRemover addRemover;
};
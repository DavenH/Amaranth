#pragma once

#include <App/Doc/Savable.h>
#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/ParameterGroup.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/InsetLabel.h>

#include "../TourGuide.h"
#include "../Widgets/Controls/ControlsPanel.h"
#include "../Widgets/Controls/TitleBackingCont.h"
#include "../VertexPanels/EffectPanel.h"
#include "../Widgets/Controls/MeshSelectionClient.h"
#include "../../Audio/Effects/WaveShaper.h"
#include "../../Util/CycleEnums.h"

template<class Mesh> class MeshSelector;

class WaveshaperUI :
        public EffectPanel
    ,	public MeshSelectionClient<Mesh>
    ,	public Button::Listener
    ,	public ComboBox::Listener
    ,	public ParameterGroup::Worker
    ,	public Savable
    ,	public TourGuide
{
public:
    explicit WaveshaperUI(SingletonRepo* repo);

    void init() override;

    /* Accessors */
    String getKnobName(int index) const;
    void setEffectEnabled(bool enabled);
    bool isEffectEnabled() const override { return isEnabled; }
    bool isCurrentMeshActive() override { return isEnabled; }
    ControlsPanel* getControlsPanel() 	{ return &controls; }

    /* UI */
    void preDraw() override;
    void postCurveDraw() override;
    void showCoordinates() override;
    void panelResized() override;

    void doGlobalUIUpdate(bool force) override;
    void reduceDetail() override;
    void restoreDetail() override;
    bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) override;
    void updateDspSync() override;
    void doLocalUIUpdate() override;
    bool shouldTriggerGlobalUpdate(Slider* slider) override;

    /* Events */
    void buttonClicked(Button* button) override;
    void comboBoxChanged(ComboBox* box) override;
    void exitClientLock() override;
    void enterClientLock() override;

    /* Persistence */
    void writeXML(XmlElement* element) const override;
    bool readXML(const XmlElement* element) override;

    /* Mesh selection */
    void setMeshAndUpdate	(Mesh* mesh);
    void setCurrentMesh		(Mesh* mesh) override;
    void previewMesh		(Mesh* mesh) override;
    void previewMeshEnded	(Mesh* oldMesh) override;
    void doubleMesh() override;

    Mesh* getCurrentMesh() override;
    Component* getComponent(int which) override;
    CriticalSection& getClientLock();

    int getUpdateSource() override { return UpdateSources::SourceWaveshaper; }
    int getLayerType() override { return layerType; }

private:
    static const int oversampFactors[5];

    bool isEnabled;

    InsetLabel 		title;
    ControlsPanel 	controls;
    TitleBacking 	titleBacking;
    IconButton 		enabledButton;
    ComboBox 		oversampleBox;

    Ref<Waveshaper> waveshaper;
    std::unique_ptr<MeshSelector<Mesh> > selector;

    Label nameLabel;
    Label nameLabelA;
    Label oversampLabel;
    Label labels[Waveshaper::numWaveshaperParams];

};

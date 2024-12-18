#pragma once

#include <App/Doc/Savable.h>
#include <Obj/Ref.h>
#include <Obj/ColorGradient.h>
#include <UI/ParameterGroup.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/InsetLabel.h>

#include "../TourGuide.h"
#include "../Widgets/Controls/ControlsClient.h"
#include "../Widgets/Controls/Spacers.h"
#include "../Widgets/Controls/TitleBackingCont.h"
#include "../Widgets/Controls/PanelControls.h"
#include "../VertexPanels/EffectPanel.h"
#include "../../Audio/Effects/IrModeller.h"
#include "../../UI/Widgets/Controls/MeshSelectionClient.h"

template<class Mesh> class MeshSelector;
class PulloutComponent;
class RetractableCallout;
class IrModeller;
class ConvTest;
class SingletonRepo;
class Knob;

class IrModellerUI:
        public Savable
    ,	public TourGuide
    ,	public ControlsClient
    ,	public Button::Listener
    ,	public EffectPanel
    ,	public ParameterGroup::Worker
    ,	public MeshSelectionClient<Mesh>
{
public:
    explicit IrModellerUI(SingletonRepo* repo);
    ~IrModellerUI() override;

    void init() override;
    void initControls();

    /* Accessors */
    String getKnobName(int index) const;
    bool isEffectEnabled() const override { return isEnabled; }
    bool isCurrentMeshActive() override	 { return isEnabled; }
    int getLayerType() override 			 { return layerType; }
    void setEffectEnabled(bool is);

    /* UI */
    void preDraw() override;
    void postCurveDraw() override;
    void panelResized() override;
    void showCoordinates() override;
    void doGlobalUIUpdate(bool force) override;
    void doLocalUIUpdate() override;
    void reduceDetail() override;
    void restoreDetail() override;
    void updateDspSync() override;
    bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) override;
    bool shouldTriggerGlobalUpdate(Slider* slider) override;
    bool paramTriggersAggregateUpdate(int knobIndex);

    /* Events */
    void buttonClicked(Button* button) override;
    void setWaveImpulsePath(const String& filename) { waveImpulsePath = filename; }
    void modelLoadedWave();
    void loadWaveFile();
    void deconvolve();

    /* Persistence */
    void writeXML(XmlElement* element) const override;
    bool readXML(const XmlElement* element) override;

    /* Mesh selection */
    Mesh* getCurrentMesh() override;
    void setCurrentMesh(Mesh* mesh) override;
    void previewMesh(Mesh* mesh) override;
    void previewMeshEnded(Mesh* oldMesh) override;
    void setMeshAndUpdate(Mesh* mesh, bool repaint = true);
    void setMeshAndUpdateNoRepaint(Mesh* mesh);
    void exitClientLock() override;
    void enterClientLock() override;
    void doubleMesh() override;
    void doZoomAction(int action);

    void createScales() override;
    void drawScales() override;

    /* Tutorial */
    Component* getComponent(int which) override;

private:
    bool isEnabled;

    Ref<IrModeller> irModeller;
    std::unique_ptr<MeshSelector<Mesh>> selector;

    String waveImpulsePath;
    ColorGradient gradient;

    IconButton openWave;
    IconButton removeWave;
    IconButton modelWave;

    IconButton impulseMode;
    IconButton freqMode;
    IconButton attkZoomIcon;
    IconButton zoomOutIcon;

    Label bufSizeLabel;
    Label nameLabel;
    Label nameLabelA;
    Label labels[IrModeller::numTubeParams];

    Ref<HSlider> lengthSlider;
    Ref<HSlider> gainSlider;
    Ref<HSlider> hpSlider;

    ClearSpacer spacer8, spacer2;

    std::unique_ptr<PulloutComponent> 	wavePO;
    std::unique_ptr<PulloutComponent> 	zoomPO;
    std::unique_ptr<PulloutComponent> 	modePO;

    std::unique_ptr<RetractableCallout> 	waveCO;
    std::unique_ptr<RetractableCallout> 	zoomCO;
    std::unique_ptr<RetractableCallout> 	modeCO;

    InsetLabel title;
    TitleBacking titleBacking;

    friend class ConvTest;

};

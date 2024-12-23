#pragma once

#include <list>
#include <vector>

#include <App/MeshLibrary.h>
#include <Audio/SmoothedParameter.h>
#include <Obj/ColorGradient.h>
#include <Obj/Ref.h>
#include <UI/AsyncUIUpdater.h>
#include <UI/Panels/Panel3D.h>
#include <UI/ParameterGroup.h>
#include <UI/Widgets/IconButton.h>

#include "../TourGuide.h"
#include "../Widgets/Controls/ControlsClient.h"
#include "../Widgets/Controls/LayerSelectionClient.h"
#include "../Widgets/Controls/PanelControls.h"
#include "../Widgets/Controls/Spacers.h"

using std::list;
using std::vector;

class RetractableCallout;
class PulloutComponent;
class Knob;
class Spectrum2D;
class HSlider;

template<class Mesh> class MeshSelector;

class Spectrum3D:
        public Panel3D
    ,	public Panel3D::DataRetriever
    ,	public Button::Listener
    ,	public Slider::Listener
    ,	public ParameterGroup::Worker
    ,	public LayerSelectionClient
    ,	public ControlsClient
    ,	public AsyncUIUpdater
    ,	public TourGuide
    ,	public Savable {
public:
    enum LayerMode { Additive, Subtractive };

    explicit Spectrum3D(SingletonRepo* repo);
    void init() override;

    /* UI */
    void drawPlaybackLine();

    void reset() override;
    void panelResized() override;
    bool haveAnyValidLayers(bool isMags, bool haveAnyValidTimeLayers);

    /* Events */
    void layerChanged() override;
    void updateColours();
    void populateLayerBox();
    void enablementsChanged();
    bool validateScratchChannels();
    void setIconHighlightImplicit();
    void changedToOrFromTimeDimension();
    void triggerButton(int button);
    void scratchChannelSelected(int channel);
    void updateBackground(bool onlyVerticalBackground) override;
    void modeChanged(bool isMags, bool updateInteractors);

    void buttonClicked(Button* button) override;
    void sliderValueChanged(Slider* slider) override;

    /* Knob Events */
    bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) override;
    bool shouldTriggerGlobalUpdate(Slider* slider) override;
    void restoreDetail() override;
    void reduceDetail() override;
    void doGlobalUIUpdate(bool force) override;
    void updateKnobValue();
    void updateSmoothedParameters(int deltaSamples);
    void updateScratchComboBox();
    void updateLayerSmoothedParameters(int voiceIndex, int deltaSamples);
    void updateSmoothParametersToTarget(int voiceIndex);
    void setLayerParameterSmoothing(int voiceIndex, bool smooth);

    /* Persistence */
    void writeXML(XmlElement* element) const override;
    bool readXML(const XmlElement* element) override;

    /* Accessors */
    void setPan(int layerIdx, bool isFreq, float value);
    void setDynRange(int layerIdx, bool isFreq, float value);
    int getLayerScratchChannel() override;
    int getNumActiveLayers();

    Buffer<float> getColumnArray() override;
    const vector<Column>& getColumns() override;

    MeshLibrary::Properties* getPropertiesForLayer(int layerIdx, bool isFreq);
    MeshLibrary::Properties* getCurrentProperties();
    MeshLibrary::LayerGroup& getCurrentGroup();

    Component* 			getComponent(int which) override;
    vector<Color>& 		getGradientColours() override;
    int 				getLayerType() override;

    int& 				getScaleFactor() 	{ return scaleFactor; 	}
    CriticalSection& 	getLayerLock() 		{ return layerLock; 	}

    static float calcPhaseOffsetScale(float value) 	{ return expf(5.f * value); }
    static float calcDynamicRangeScale(float value) { return powf(2, 12 * value - 4); }

private:
    enum Knobs { Pan, DynRange };

    int scaleFactor;

    ClearSpacer spacer6;
    CriticalSection layerLock;
    ColorGradient 	phaseGradient;

    IconButton phaseIcon;
    IconButton magsIcon;
    IconButton additiveIcon;
    IconButton subtractiveIcon;

    StringFunction dynStr;

    Ref<HSlider> dynRangeKnob;
    Ref<HSlider> layerPan;
    Ref<MeshLibrary> meshLib;
    Ref<Spectrum2D> spect2D;

    std::unique_ptr<PulloutComponent> 	operPO;
    std::unique_ptr<PulloutComponent> 	modePO;
    std::unique_ptr<RetractableCallout> operCO;
    std::unique_ptr<RetractableCallout> modeCO;
    std::unique_ptr<MeshSelector<Mesh>> meshSelector;
};

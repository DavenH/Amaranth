#pragma once

#include <iostream>
#include <App/Doc/Savable.h>
#include <UI/AsyncUIUpdater.h>
#include <UI/Panels/Panel3D.h>
#include <UI/ParameterGroup.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/Knob.h>
#include "../TourGuide.h"
#include "../Widgets/Controls/ControlsClient.h"
#include "../Widgets/Controls/LayerSelectionClient.h"
#include "../Widgets/Controls/Spacers.h"
#include "../Widgets/Controls/PanelControls.h"

using std::cout;
using std::endl;

#ifndef GL_BGRA_EXT
     #define GL_BGRA_EXT 0x80e1
#endif

class EditWatcher;
class Mesh;
class WaveformInter3D;
template<class Mesh> class MeshSelector;

class Waveform3D:
        public Panel3D
    ,	public Panel3D::DataRetriever
    ,	public Slider::Listener
    ,	public Button::Listener
    ,	public ParameterGroup::Worker
    ,	public LayerSelectionClient
    ,	public ControlsClient
    ,	public AsyncUIUpdater
    ,	public TourGuide
    ,	public Savable {
public:
    explicit Waveform3D(SingletonRepo*);
    ~Waveform3D() override;

    void init() override;

    /* UI */
    void panelResized() override;
    void reset() override;

    /* Events */
    bool shouldDrawGrid();
    bool shouldTriggerGlobalUpdate(Slider* slider) override;
    bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) override;
    bool validateScratchChannels();

    Component* getComponent(int which) override;

    int getLayerScratchChannel() override;
    int getNumActiveLayers();

    void doGlobalUIUpdate(bool force) override;
    void layerChanged() override;
    void populateLayerBox();
    void reduceDetail() override;
    void restoreDetail() override;
    void scratchChannelSelected(int channel);
    void setLayerParameterSmoothing(int voiceIndex, bool smooth);
    void triggerButton(int button);
    void updateLayerSmoothedParameters(int voiceIndex, int deltaSamples);
    void updateScratchComboBox();
    void updateSmoothedParameters(int deltaSamples);
    void updateSmoothParametersToTarget(int voiceIndex);
    void zoomUpdated();
    void doZoomExtra(bool commandDown) override;

    void buttonClicked(Button* button) override;
    void sliderValueChanged (Slider* slider) override;

    Buffer<float> getColumnArray() override;
    const vector<Column>& getColumns() override;

    /* Accessors */
    int getLayerType() override;
    int getWindowWidthPixels();

    CriticalSection& 	getLayerLock() { return layerLock; }

    void setPan(int layerIdx, float unitValue);
    void setKnobValuesImplicit();

    /* Persistence */
    bool readXML(const XmlElement* element) override;
    void writeXML(XmlElement* element) const override;

private:
    MeshLibrary::Properties* getCurrentProperties();

    enum Knobs { Pan, Fine };

    IconButton model;
    IconButton deconvolve;
    ClearSpacer spacer8;

    CriticalSection layerLock;

    Ref<HSlider> layerFine;
    Ref<HSlider> layerPan;
    Ref<WaveformInter3D> surfInteractor;
    std::unique_ptr<MeshSelector<Mesh> > 	meshSelector;
};

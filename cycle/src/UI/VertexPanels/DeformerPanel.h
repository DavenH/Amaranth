#pragma once

#include <Curve/IDeformer.h>
#include <Curve/Mesh.h>
#include <Obj/Ref.h>
#include <UI/Widgets/Knob.h>

#include "EffectPanel.h"
#include "../TourGuide.h"
#include "../Widgets/Controls/ControlsClient.h"
#include "../Widgets/Controls/LayerSelectionClient.h"
#include "../Widgets/Controls/MeshSelectionClient.h"
#include "../Widgets/Controls/MeshSelector.h"
#include "../Widgets/HSlider.h"

class MeshLibrary;

class DeformerPanel :
        public EffectPanel
    ,	public Slider::Listener
    ,	public Button::Listener
    ,	public LayerSelectionClient
    ,	public MeshSelectionClient<Mesh>
    ,	public Savable
    ,	public TourGuide
    ,	public ControlsClient
    ,	public IDeformer
{
public:
    enum { tableSize = 8192, tableModulo = tableSize - 1 };

    explicit DeformerPanel(SingletonRepo* repo);

    bool isEffectEnabled() const override;
    bool setGuideBuffers();
    int getNumGuides();
    int getTableDensity(int index) override;

    Mesh* getCurrentMesh() override;
    Component* getComponent(int which) override;

    void addNewLayer(bool doUpdate);
    void buttonClicked(Button* button) override;
    void doubleMesh() override;
    void enterClientLock() override;
    void exitClientLock() override;
    void init() override;
    void layerChanged() override;
    void panelResized() override;
    void preDraw() override;
    void previewMesh(Mesh* mesh) override;
    void previewMeshEnded(Mesh* mesh) override;
    void rasterizeAllTables();
    void rasterizeTable();
    void reset() override;
    void setCurrentMesh(Mesh* mesh) override;
    void setMeshAndUpdate(Mesh *mesh);
    void showCoordinates() override;
    void sliderDragEnded(Slider* slider) override;
    void sliderDragStarted(Slider* slider) override;
    void sliderValueChanged(Slider* slider) override;
    void triggerButton(int id);
    void updateDspSync() override;
    void updateKnobsImplicit();

    int getLayerType() override { return layerType; }

    bool readXML(const XmlElement* element) override;
    void writeXML(XmlElement* element) const override;

    float getTableValue(int guideIndex, float progress, const IDeformer::NoiseContext& context) override
    {
        jassert(guideIndex < guideTables.size());
        if (guideIndex >= guideTables.size()) {
            return 0;
}

        float position  = progress * (DeformerPanel::tableSize - 1);
        int idx 		= (int) position;

        GuideProps& props = guideTables[guideIndex];

        int phaseOffset = (context.phaseOffset & tableModulo - tableSize / 2) * props.phaseOffsetLevel;

        return props.table[(idx + phaseOffset) & tableModulo] +
                props.noiseLevel * noiseArray[(context.noiseSeed + props.seed) & tableModulo] +
                noiseArray[context.vertOffset] * props.vertOffsetLevel;
    }

    Buffer<Float32> getTable(int index)
    {
        if (index < 0) {
            return Buffer<Float32>();
}

        return guideTables[index].table;
    }

    void sampleDownAddNoise(int index, Buffer<float> dest,
                            const IDeformer::NoiseContext& context);

private:
    class GuideProps
    {
    public:
        GuideProps(DeformerPanel* pnl, int idx,
                    float noiseLevel, float offsetLevel, float phaseLevel,
                    int seed) :
            noiseLevel(noiseLevel),
            vertOffsetLevel(offsetLevel),
            phaseOffsetLevel(phaseLevel),
            seed(seed),
            panel(pnl)
        {
        }

        int seed;
        float noiseLevel, vertOffsetLevel, phaseOffsetLevel;
        Ref<DeformerPanel> panel;
        Buffer<Float32> table;
    };

    float samplingInterval;

    Random random;
    Ref<MeshLibrary> meshLib;

    ScopedAlloc<Float32> constMemory;
    ScopedAlloc<Float32> dynMemory;
    Buffer<float> noiseArray;
    Buffer<float> phaseMoveBuffer;
    vector<GuideProps> guideTables;

    std::unique_ptr<MeshSelector<Mesh> > meshSelector;

    HSlider noise;
    HSlider vertOffset;
    HSlider phaseOffset;
};
#pragma once

#include <ippdefs.h>
#include <Curve/IDeformer.h>
#include <Curve/Mesh.h>
#include <Obj/Ref.h>
#include <UI/Widgets/Knob.h>

#include "EffectPanel.h"
#include "../TourGuide.h"
#include "../Widgets/Controls/ControlsClient.h"
#include "../Widgets/Controls/LayerSelectionClient.h"
#include "../Widgets/Controls/LayerSelectorPanel.h"
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

	DeformerPanel(SingletonRepo* repo);
	~DeformerPanel() override;

	bool isEffectEnabled() const override;
	bool setGuideBuffers();
	int getNumGuides();
	int getTableDensity(int index) override;

	Mesh* getCurrentMesh();
	Component* getComponent(int which);

	void addNewLayer(bool doUpdate);
	void buttonClicked(Button* button) override;
	void doubleMesh() override;
	void enterClientLock() override;
	void exitClientLock();
	void init();
	void layerChanged();
	void panelResized();
	void preDraw();
	void previewMesh(Mesh* mesh);
	void previewMeshEnded(Mesh* mesh);
	void rasterizeAllTables();
	void rasterizeTable();
	void reset();
	void setCurrentMesh(Mesh* mesh);
	void setMeshAndUpdate(Mesh *mesh);
	void showCoordinates();
	void sliderDragEnded(Slider* slider);
	void sliderDragStarted(Slider* slider);
	void sliderValueChanged(Slider* slider);
	void triggerButton(int id);
	void updateDspSync();
	void updateKnobsImplicit();

	int getLayerType() { return layerType; }

	bool readXML(const XmlElement* element);
	void writeXML(XmlElement* element) const;

	float getTableValue(int guideIndex, float progress, const IDeformer::NoiseContext& context)
	{
		jassert(guideIndex < guideTables.size());
		if(guideIndex >= guideTables.size())
			return 0;

		float position  = progress * (DeformerPanel::tableSize - 1);
		int idx 		= (int) position;

		GuideProps& props = guideTables[guideIndex];

		int phaseOffset = (context.phaseOffset & tableModulo - tableSize / 2) * props.phaseOffsetLevel;

		return props.table[(idx + phaseOffset) & tableModulo] +
				props.noiseLevel * noiseArray[(context.noiseSeed + props.seed) & tableModulo] +
				noiseArray[context.vertOffset] * props.vertOffsetLevel;
	}

	Buffer<Ipp32f> getTable(int index)
	{
		if(index < 0)
			return Buffer<Ipp32f>();

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
		Buffer<Ipp32f> table;
	};

	float samplingInterval;

	Random random;
	Ref<MeshLibrary> meshLib;

	ScopedAlloc<Ipp32f> constMemory;
	ScopedAlloc<Ipp32f> dynMemory;
	Buffer<float> noiseArray;
	Buffer<float> phaseMoveBuffer;
	vector<GuideProps> guideTables;

	std::unique_ptr<MeshSelector<Mesh> > meshSelector;

	HSlider noise;
	HSlider vertOffset;
	HSlider phaseOffset;
};
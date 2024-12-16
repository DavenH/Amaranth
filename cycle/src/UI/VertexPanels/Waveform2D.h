#pragma once

#include <Obj/Ref.h>
#include <UI/Panels/Panel2D.h>

class SingletonRepo;
class PlaybackPanel;

class Waveform2D :
		public Panel2D {
public:
	explicit Waveform2D(SingletonRepo* repo);

	bool shouldDrawCurve() override;
	int getLayerScratchChannel() override;
	void componentChanged() override;
	void drawBackground(bool fillBackground) override;
	void drawCurvesAndSurfaces() override;
	void drawMouseHint();
	void fillCurveFromGrid();
	void init() override;
	void postCurveDraw() override;
	void preDraw() override;

	void doZoomExtra(bool commandDown) override;
	int initXYArrays(Buffer<float> one, Buffer<float> two, float portion);

private:
	Ref<PlaybackPanel> position;
	ScopedAlloc<Ipp32f> downsampleBuf;

	void drawHistory();
	void drawIfftCycle();
};

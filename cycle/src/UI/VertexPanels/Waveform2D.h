#pragma once

#include <Obj/Ref.h>
#include <UI/Panels/Panel2D.h>

class SingletonRepo;
class PlaybackPanel;

class Waveform2D :
		public Panel2D {
public:
	Waveform2D(SingletonRepo* repo);

	bool shouldDrawCurve();
	int getLayerScratchChannel();
	void componentChanged();
	void drawBackground(bool fillBackground);
	void drawCurvesAndSurfaces();
	void drawMouseHint();
	void fillCurveFromGrid();
	void init();
	void postCurveDraw();
	void preDraw();

	void doZoomExtra(bool commandDown);
	int initXYArrays(Buffer<float> one, Buffer<float> two, float portion);

private:
	Ref<PlaybackPanel> position;
	ScopedAlloc<Ipp32f> downsampleBuf;

	void drawHistory();
	void drawIfftCycle();
};

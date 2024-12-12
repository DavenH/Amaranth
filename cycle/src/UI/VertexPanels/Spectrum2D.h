#pragma once

#include <Array/ScopedAlloc.h>
#include <Obj/Ref.h>
#include <UI/Panels/Panel2D.h>
#include <Util/MicroTimer.h>

class Spectrum3D;
class Console;
class SpectrumInter2D;
class PlaybackPanel;

class Spectrum2D :
        public Panel2D {
public:
	void init();

	void preDraw();
	void drawBackground(bool fillBackground);
	void drawPartials();
	void drawThres();
	void drawHistory();
	void drawScales();
	void createScales();
	int getLayerScratchChannel();

	Spectrum2D(SingletonRepo* repo);
	virtual ~Spectrum2D();

private:
	Ref<PlaybackPanel> 		position;
	Ref<Spectrum3D> 		spectrum3D;
	Ref<SpectrumInter2D> 	f2Interactor;
	Ref<Console> 			console;
	ScopedAlloc<Ipp32f> 	decibelLines;

//	void doPostZoomRatioChange(const MouseEvent& e, float oldZoom);
};

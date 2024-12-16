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
	void init() override;

	void preDraw() override;
	void drawBackground(bool fillBackground) override;
	void drawPartials();
	void drawThres();
	void drawHistory();
	void drawScales() override;
	void createScales() override;
	int getLayerScratchChannel() override;

	explicit Spectrum2D(SingletonRepo* repo);
	~Spectrum2D() override = default;

private:
	Ref<PlaybackPanel> 		position;
	Ref<Spectrum3D> 		spectrum3D;
	Ref<SpectrumInter2D> 	f2Interactor;
	Ref<Console> 			console;
	ScopedAlloc<Ipp32f> 	decibelLines;

//	void doPostZoomRatioChange(const MouseEvent& e, float oldZoom);
};

#include <iostream>

#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Array/Buffer.h>
#include <Audio/PitchedSample.h>
#include <Inter/Interactor.h>
#include <Obj/ColorGradient.h>
#include <UI/Panels/CommonGfx.h>
#include <UI/Panels/Panel2D.h>
#include <UI/Panels/ScopedGL.h>

#include "Spectrum3D.h"
#include "Waveform2D.h"
#include "../Panels/Morphing/MorphPanel.h"
#include "../VertexPanels/Waveform3D.h"
#include "../../UI/Panels/PlaybackPanel.h"
#include "../../UI/VisualDsp.h"
#include "../../Util/CycleEnums.h"

using namespace gl;

Waveform2D::Waveform2D(SingletonRepo* repo) :
		SingletonAccessor(repo, "Waveform2D")
	,	Panel2D			 (repo, "Waveform2D", true, true)
	,	downsampleBuf	 (2048) {
	vertPadding 			= 0;
	curveIsBipolar 			= true;
	paddingLeft 			= paddingRight = 0;
	cyclicLines 			= true;
	doesDrawMouseHint 		= true;
//	drawsCurvesPostFFT 		= true; // TODO implement shouldDrawCurves
	backgroundTimeRelevant 	= false;

	colorA 					= Color(0.8f, 0.8, 0.9f, 0.6f);
	colorB 					= Color(0.8f, 0.8, 0.9f, 0.6f);

	createNameImage("Waveshape Editor");
}


void Waveform2D::init() {
    position = &getObj(PlaybackPanel);
}

void Waveform2D::preDraw() {
	if (getSetting(Waterfall))
		drawHistory();
}

void Waveform2D::drawCurvesAndSurfaces() {
	if ((getSetting(DrawWave) || getSetting(ViewStage) > ViewStages::PreProcessing) && !getSetting(Waterfall)) {
		fillCurveFromGrid();
	}

	// fills curve from mesh
	Panel2D::drawCurvesAndSurfaces();

	if (getSetting(ViewStage) > ViewStages::PreProcessing || getSetting(DrawWave))
		drawIfftCycle();
}

void Waveform2D::drawBackground(bool fillBackground) {
	Panel::drawBackground();

	gfx->setCurrentColour(0.28f, 0.28f, 0.32f, 0.2f);
	gfx->fillRect(0, 0, 1.f, 0.25f, true);
	gfx->fillRect(0, 0.75f, 1.f, 1.f, true);
}

void Waveform2D::postCurveDraw() {
}

void Waveform2D::fillCurveFromGrid() {
	const vector<Column>& timeColumns = getObj(VisualDsp).getTimeColumns();

	if (timeColumns.size() < 2 || !getObj(Waveform3D).shouldDrawGrid())
		return;

	Color c = colorA.withAlpha(getSetting(DrawWave) ? 0.2f : isMeshEnabled() ? 0.5f : 0.3f);
	float baseAlpha = c.alpha() * 0.2f;

	int numCols 	= timeColumns.size() - 1;
	float fIndex 	= position->getX() * (numCols);
	float remainder = fIndex - int(fIndex);
	float baseY 	= sy(0.5f);

	const Column& one = timeColumns[int(fIndex)];
	const Column& two = timeColumns[jmin(numCols, int(fIndex) + 1)];

	initXYArrays(one, two, remainder);
	int size = jmin(one.size(), two.size());

	cBuffer.ensureSize(size);
	Buffer<float> a = cBuffer.withSize(size);

	xy.x = xBuffer.withSize(size);
	xy.y = yBuffer.withSize(size);

	a.mul(xy.y, 0.25f).abs().subCRev(0.5f).mul(c.alpha());

	applyScaleX(xy.x);
	applyScaleY(xy.y);

	ColorPos curr;
	vector<ColorPos> positions;

	for(int i = 0; i < size; ++i) {
		curr.update(xy[i], c.withAlpha(a[i]));
		positions.push_back(curr);
	}

	gfx->fillAndOutlineColoured(positions, baseY, baseAlpha, true, true);
}

int Waveform2D::initXYArrays(Buffer<float> one, Buffer<float> two, float portion) {
    Buffer<float>& smaller = one.size() < two.size() ? one : two;
	Buffer<float>& larger =  one.size() < two.size() ? two : one;

	int size = smaller.size();

	if(size < 2)
		return 0;

	prepareBuffers(size);

    if (larger.size() >= 2 * smaller.size()) {
		downsampleBuf.ensureSize(larger.size());
		Buffer<float> downBuf = downsampleBuf.withSize(smaller.size());

		downBuf.downsampleFrom(larger);

		larger = downBuf;
	}

	xBuffer.withSize(size).ramp();

	Buffer<float> y = yBuffer.withSize(size);

	if(portion == 0.f)
        one.copyTo(y);
    else {
		y.zero().addProduct(one, 1 - portion).addProduct(two, portion);
	}

	y.add(0.5f);

	return size;
}

void Waveform2D::drawIfftCycle() {
    const vector<Column>& ifftColumns = getObj(VisualDsp).getTimeColumns();

	if(ifftColumns.empty()) {
		return;
	}

	int maxRow 		= (int) ifftColumns.size() - 1;
	float fIndex 	= position->getProgress() * maxRow;
	int iIndex 		= (int) fIndex;
	float remainder = fIndex - iIndex;
	int secondCol 	= jmin(maxRow, iIndex + 1);
	int size 		= initXYArrays(ifftColumns[iIndex], ifftColumns[secondCol], remainder);

	if(! size) {
		return;
	}

	bool reduceAlpha = getSetting(DrawWave) == false && interactor->mouseFlag(MouseOver);
	gfx->enableSmoothing();
	gfx->setCurrentColour(colorA.withAlpha(reduceAlpha ? 0.5f : 1.f));
	gfx->drawLineStrip(xy, true, true);
}

void Waveform2D::drawHistory() {
	Panel3D::Renderer* renderer = getObj(Waveform3D).getRenderer();
	const vector<Column>& columns = renderer->getColumns();
	float width 	= getObj(MorphPanel).getDepth(getSetting(CurrentMorphAxis));
	int numCols 	= (int) columns.size();

	ScopedLock sl(renderer->getGridLock());

	if(numCols == 0)
		return;

	float alpha;
	float xInc 		= 0.04f * width;
	float progress 	= position->getProgress();
	float end 		= jmin(1.f, progress + width);
	float cume 		= end;
	int columnIndex = int(end * (numCols - 1));

	gfx->setCurrentLineWidth(1.f);
	gfx->enableSmoothing();

	bool reduceAlpha = getSetting(DrawWave) == false && interactor->mouseFlag(MouseOver);

	float alphaScale = reduceAlpha ? 0.2f : 0.4f;

	{
		ScopedEnableClientState glState(GL_VERTEX_ARRAY);

        while (cume > progress) {
			alpha = 1.1f - (cume - progress) / (end - progress);

			gfx->setCurrentColour(0.45f, 0.5f, 0.75f, alphaScale * alpha);

			while(columnIndex > 0 && columns[columnIndex - 1].x > cume)
				--columnIndex;

			int firstCol = jmax(0, columnIndex - 1);
			jassert(columnIndex < numCols && firstCol < numCols);

			float diffX 	= jmax(0.f, cume) - columns[firstCol].x;
			float diffCol 	= columns[columnIndex].x - columns[firstCol].x;
			float portion 	= diffX / diffCol;

			initXYArrays(columns[firstCol], columns[columnIndex], portion);

			gfx->drawLineStrip(xy, false, true);

			cume -= xInc;
		}
	}
}

void Waveform2D::drawMouseHint() {
    if (interactor->state.actionState != PanelState::BoxSelecting
        && !ModifierKeys::getCurrentModifiers().isCommandDown()
        && getSetting(Tool) == Tools::Selector) {
		// gfx->drawVerticalLine();
	}
}

bool Waveform2D::shouldDrawCurve() {
    return true; //getSetting(ViewStage) == ViewStages::PreProcessing;
//			interactor->mouseFlag(MouseOver) || ;
}

int Waveform2D::getLayerScratchChannel() {
    return getObj(Waveform3D).getLayerScratchChannel();
}

void Waveform2D::componentChanged() {
    Panel::componentChanged();
    comp->setMouseCursor(MouseCursor::CrosshairCursor);
}

void Waveform2D::doZoomExtra(bool commandDown) {
    if (commandDown) {
        zoomPanel->rect.y = 0.25f;
        zoomPanel->rect.h = 0.5f;
    }
}

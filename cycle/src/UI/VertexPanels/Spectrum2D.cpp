#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Array/ScopedAlloc.h>
#include <Thread/LockTracer.h>
#include <UI/MiscGraphics.h>
#include <UI/Panels/ScopedGL.h>
#include <UI/Panels/Texture.h>
#include <Util/NumberUtils.h>
#include <Util/Util.h>
#include <Util/LogRegions.h>

#include "Spectrum2D.h"
#include "Spectrum3D.h"
#include "../Panels/Morphing/MorphPanel.h"
#include "../Panels/PlaybackPanel.h"
#include "../../Audio/AudioSourceRepo.h"
#include "../../Curve/GraphicRasterizer.h"
#include "../../Inter/SpectrumInter2D.h"
#include "../../UI/Panels/Console.h"
#include "../../UI/VertexPanels/Waveform2D.h"
#include "../../UI/VisualDsp.h"
#include "../../Util/CycleEnums.h"

using namespace gl;

Spectrum2D::Spectrum2D(SingletonRepo* repo) : 
		SingletonAccessor(repo, "Spectrum2D")
	,	Panel2D(repo, "Spectrum2D", true, true)
	,	decibelLines(24) {
}


Spectrum2D::~Spectrum2D() {
}


void Spectrum2D::init() {
    position 		= &getObj(PlaybackPanel);
	spectrum3D 		= &getObj(Spectrum3D);
	f2Interactor 	= &getObj(SpectrumInter2D);
	console 		= dynamic_cast<Console*>(&getObj(IConsole));
	interactor 		= f2Interactor;

	float margin = getRealConstant(SpectralMargin);

	vertPadding = 0;
	paddingRight = 0;
	zoomPanel->rect.w = 0.95f;
	zoomPanel->rect.x = 0.05f;
	zoomPanel->rect.xMinimum = -margin;
	zoomPanel->rect.xMaximum = 1 + margin;
	zoomPanel->tendZoomToTop = false;

	createNameImage("Magn. Spectrum", false);
	createNameImage("Phase Spectrum", true);

	double value = 1.f;

	for(int i = 0; i < decibelLines.size(); ++i) {
		decibelLines[i] = (value *= 0.5);
	}

	Arithmetic::applyLogMapping(decibelLines, getRealConstant(FFTLogTensionAmp));
}

void Spectrum2D::preDraw() {
    drawThres();
    drawPartials();
}

void Spectrum2D::drawBackground(bool fillBackground) {
    const vector <Column>& columns = getObj(VisualDsp).getFreqColumns();
    int index = position->getProgress() * (columns.size() - 1);

	if(index >= columns.size() || getWidth() == 0 || getHeight() == 0)
		return;

	const Column& column = columns[index];

	int midiKey = column.midiKey;
	if(midiKey < 0)
		midiKey = getObj(MorphPanel).getCurrentMidiKey();

	Buffer<float> ramp = getObj(LogRegions).getRegion(midiKey);

	xBuffer.ensureSize(ramp.size());
	yBuffer.ensureSize(decibelLines.size());
	cBuffer.ensureSize(ramp.size());

	Buffer<float> scaledRamp 	= xBuffer.withSize(ramp.size());
	Buffer<float> scaledDB 		= yBuffer.withSize(decibelLines.size());
	Buffer<float> sProgress 	= cBuffer.withSize(ramp.size());

	ramp.copyTo(scaledRamp);
	decibelLines.copyTo(scaledDB);

	applyScaleX(scaledRamp);
	applyScaleY(scaledDB);

	cBuffer.ensureSize(ramp.size());

	sProgress.ramp(0.f, 1 / float(sProgress.size()));

	gfx->setCurrentColour(0.065f, 0.065f, 0.065f);
	gfx->disableSmoothing();
	gfx->fillRect(0, 0, getWidth(), getHeight(), false);

	Range<float> limits = interactor->vertexLimits[interactor->dims.x];

    if (limits.getStart() < 0) {
        gfx->setCurrentColour(0.1f, 0.1f, 0.1f);
        gfx->fillRect(sx(limits.getStart()), 0, sx(0), getHeight(), false);
    }

    if (limits.getEnd() > 1) {
		gfx->setCurrentColour(0.1f, 0.1f, 0.1f);
		gfx->fillRect(sx(1), 0, sx(limits.getEnd()), getHeight(), false);
	}

	gfx->checkErrors();

	float y0 = sy(0);
	float y1 = sy(1);
	float x0 = sx(0);
	float x1 = sx(1);

    if (getSetting(MagnitudeDrawMode)) {
        float colorOne, colorTwo, progress;
        Color c1, c2;

        gfx->setCurrentLineWidth(1.f);
        {
            for (int i = 0; i < (int) sProgress.size() - 7; i += 4) {
                float base 	= i % 16 == 0 ? 0.165f : 0.14f;
				progress 	= sProgress[i];
				colorOne 	= base - 0.08f * progress;
				c1 			= Color(colorOne);

				colorTwo 	= 0.07f - 0.01f * progress;
				c2 			= Color(colorTwo);

				gfx->drawLine(scaledRamp[i], y0, scaledRamp[i], y1, c1, c2);
			}
		}

		gfx->checkErrors();

		{
			cBuffer.ensureSize(decibelLines.size());
			sProgress = cBuffer.withSize(decibelLines.size());
			sProgress.ramp(1.f, -1 / float(sProgress.size()));

            for (int i = 0; i < (int) sProgress.size(); ++i) {
                progress = sProgress[i];
				colorOne 	= 0.16f - 0.06f * progress;
				c1 			= Color(colorOne);

				colorTwo 	= 0.07f - 0.005f * progress;
				c2 			= Color(colorTwo);

				gfx->drawLine(x0, scaledDB[i], x1, scaledDB[i], c1, c2);
			}
		}

		gfx->checkErrors();
	} else {
        float powScale = powf(2, -spectrum3D->getScaleFactor() * 0.5f);
        int quarterSize = ramp.size() / 4;

        yBuffer.ensureSize(quarterSize);
		xBuffer.ensureSize(quarterSize);
		cBuffer.ensureSize(quarterSize);

		Buffer<float> rampReduced 	= xBuffer.withSize(quarterSize);
		Buffer<float> phaseLines 	= yBuffer.withSize(quarterSize);
		Buffer<float> copyBuf 		= cBuffer.withSize(quarterSize);

		BufferXY tempXY;
		tempXY.x = rampReduced;
		tempXY.y = copyBuf;

		rampReduced.downsampleFrom(scaledRamp, 4);

		gfx->setCurrentColour(0.07f, 0.07f, 0.07f);

		for (int i = 0; i < rampReduced.size(); ++i)
			gfx->drawLine(rampReduced[i], y0, rampReduced[i], y1, false);

		phaseLines.ramp(1.f, 1.f).sqrt().divCRev(0.5f).mul(powScale).add(0.5f);

		int intInvScale = int(1.f / powScale) * 8;
		NumberUtils::constrain(intInvScale, 8, ramp.size());

		gfx->setCurrentLineWidth(1.f);
		gfx->setCurrentColour(0.1f, 0.1f, 0.1f);
        int j;

        for (j = 0; j < intInvScale; ++j) {
            phaseLines.copyTo(copyBuf);
            applyScaleY(copyBuf);

			if(j % 8 == 7)
				gfx->setCurrentColour(0.12f, 0.12f, 0.12f);
			else
				gfx->setCurrentColour(0.09f, 0.09f, 0.09f);

			gfx->drawLineStrip(tempXY, true, false);

			phaseLines.subCRev(1.f).copyTo(copyBuf);
			applyScaleY(copyBuf);

			gfx->drawLineStrip(tempXY, true, false);

			float scale = float(j + 2) / float(j + 1);

			phaseLines.add(-0.5f).mul(scale).add(0.5f);
		}
	}
}

void Spectrum2D::drawPartials() {
    if (getWidth() == 0 || getHeight() == 0)
        return;

    Buffer<float> ramp, scaledCol;
    int minSize;

    {
        ScopedLock sl(getObj(VisualDsp).getCalculationLock());

		const vector<Column>& columns = getObj(VisualDsp).getFreqColumns();
		int index = position->getProgress() * (columns.size() - 1);

		if(index >= columns.size())
			return;

		const Column& column = columns[index];

		int midiKey = column.midiKey;
		if(midiKey < 0)
			midiKey = getObj(MorphPanel).getCurrentMidiKey();

		ramp 	= getObj(LogRegions).getRegion(midiKey);
		minSize = jmin(ramp.size(), column.size());

		yBuffer.ensureSize(column.size());
		scaledCol = yBuffer.withSize(minSize);
		column.copyTo(scaledCol);
	}

	xBuffer.ensureSize(ramp.size());

	Buffer<float> scaledRamp = xBuffer.withSize(minSize);

	ramp.copyTo(scaledRamp);
	scaledCol.mul(0.995f);

	applyScaleX(scaledRamp);
	applyScaleY(scaledCol);

	int i = 0, start = 0;

	while(ramp[start] < zoomPanel->rect.x)
		start++;

	if(start > 0)
		start--;

	int end = minSize - 1;
	while(ramp[end] > zoomPanel->rect.w + zoomPanel->rect.x && end > 0) {
		end--;
	}

	end = jmin(minSize - 1, end + 1);
	int widthMostOne = end;

	while(widthMostOne > 1 && scaledRamp[widthMostOne] - scaledRamp[widthMostOne - 1] < 2) {
		--widthMostOne;
	}

	if(widthMostOne <= 2) {
		return;
	}

	int widthMostThree = widthMostOne;

	while(widthMostThree > 1 && scaledRamp[widthMostThree] - scaledRamp[widthMostThree - 1] < 4) {
		--widthMostThree;
	}

	int widthMostSix = widthMostThree;

	while(widthMostSix > 1 && scaledRamp[widthMostSix] - scaledRamp[widthMostSix - 1] < 7) {
		--widthMostSix;
	}

	NumberUtils::constrain<int>(start, 0, end);

	Color greyLight(0.6f, 0.6f, 0.65f, 0.35f);
	Color greyDark = greyLight.withAlpha(0.15f);

	Color lineColor(0.75f, 0.75f, 0.85f, 0.35f);
	Color highlightColor(0.3f, 0.4f, 0.8f, 0.5f);

	Color highlitBot(0.3f, 0.4f, 0.7f, 0.3f);
	Color highlitTop = highlitBot.withAlpha(0.15f);
	Color black(0.1, 0.1, 0.1, 1.f);

	float base = getSetting(MagnitudeDrawMode) ? sy(0.f) : sy(0.5f);
	float dx;
	float x, y;

	int size = ramp.size();

	int closestHarmonic = -1;

	{
		ScopedLock sl(interactor->getLock());
		closestHarmonic = f2Interactor->getClosestHarmonic();
	}

    if (comp->isMouseOver() && closestHarmonic >= 0 && closestHarmonic < minSize) {
        float top = sy(1.f);

		x 	= scaledRamp[closestHarmonic];
		dx 	= (int) scaledRamp[jmin(size - 1, closestHarmonic + 1)] - x - 2;
		y 	= scaledCol[closestHarmonic] - 1;

		gfx->fillRect(x, top, x + dx, y, highlitTop, highlitBot);

		int xOffset = int(closestHarmonic == 0);

		gfx->setCurrentColour(highlightColor);
		gfx->drawLine(x + xOffset, 	scaledCol[closestHarmonic], x + xOffset, 	base, false);
		gfx->drawLine(x + dx, 		scaledCol[closestHarmonic], x + dx, 		base, false);
	}


    for (i = start; i < widthMostSix; ++i) {
        x = scaledRamp[i];
        dx = (scaledRamp[i + 1] - x) - 2;
		y 	= (int) scaledCol[i];

		gfx->fillRect(x, y, x + dx, base, greyLight, greyDark);
	}

    for (; i < widthMostOne; ++i) {
        x = scaledRamp[i] - 1;
        dx = scaledRamp[i + 1] - scaledRamp[i];
		y = (int) scaledCol[i];

		gfx->fillRect(x, y, x + dx, base, greyLight, greyDark);
	}

    vector <ColorPos> positions;
    for (; i < end + 1; ++i) {
        ColorPos pos;
        pos.x = scaledRamp[i] - 1;
		pos.y = (int) scaledCol[i];
		pos.c = greyLight;

		positions.push_back(pos);
	}

	gfx->fillAndOutlineColoured(positions, base, 0.15f, true, false);

	gfx->disableSmoothing();
	gfx->setCurrentLineWidth(1.f);

	float negAlpha = 0;

	// top capping lines
	gfx->setCurrentColour(lineColor);

    for (i = start; i < widthMostSix; ++i) {
        x 	= scaledRamp[i];
		dx 	= (scaledRamp[i + 1] - x) - 2;
		y 	= (int) scaledCol[i];

		gfx->drawLine(x, y, x + dx, y, false);
	}

    for (; i < widthMostThree; ++i) {
        x = scaledRamp[i];
        dx = (scaledRamp[i + 1] - x) - 2;
		y = (int) scaledCol[i];

		negAlpha = (widthMostThree - i) / float(widthMostThree - widthMostSix);
		negAlpha *= negAlpha;

		gfx->setCurrentColour(lineColor.withAlpha(negAlpha * 0.4f));
		gfx->drawLine(x, y, x + dx, y, false);
	}

    // side lines up to point where bars get too thin
    gfx->setCurrentColour(lineColor);

    for (i = start; i < widthMostSix; ++i) {
        x = scaledRamp[i];
		dx 	= scaledRamp[i + 1] - x - 2;
		y 	= (int) scaledCol[i];

		gfx->drawLine(x, base, x, y, false);
		gfx->drawLine(x + dx, base, x + dx, y, false);
	}

    // when they get too thin start fading them out
    for (; i < widthMostThree; ++i) {
        negAlpha = (widthMostThree - i) / float(widthMostThree - widthMostSix);
		negAlpha *= negAlpha;

		x 	= scaledRamp[i];
		dx 	= (scaledRamp[i + 1] - x) - 1;
		y 	= (int) scaledCol[i];

		gfx->setCurrentColour(lineColor.withAlpha(negAlpha * 0.4f));
		gfx->drawLine(x, y, x, base, false);
		gfx->drawLine(x + dx - 1, y, x + dx - 1, base, false);

		gfx->setCurrentColour(black.withAlpha(negAlpha));
		gfx->drawLine(x + dx, base, x + dx, jmin((int)y, (int) scaledCol[i + 1]), false);
	}
}

void Spectrum2D::drawThres() {
    gfx->disableSmoothing();
	gfx->setCurrentLineWidth(1.f);
	gfx->setCurrentColour(0.15f, 0.15f, 0.15f);
	gfx->drawLine(0, 0.5f, 1, 0.5f, true);
}

void Spectrum2D::drawHistory() {
    int midiKey = getObj(MorphPanel).getCurrentMidiKey();
    float width = getObj(MorphPanel).getDepth(getSetting(CurrentMorphAxis));
    float alphaA;
	float blueAlpha;

	Buffer<float> ramp = getObj(LogRegions).getRegion(midiKey);
	const vector<Column>& columns = getObj(VisualDsp).getFreqColumns();

	int size 			= columns.size();
	int historySize 	= width * size;
	int playbackIndex 	= int(position->getProgress() * size);
	int start 			= playbackIndex;
	int end 			= jmin(size - 1, playbackIndex + historySize);
	historySize 		= jmin(end - start, historySize);
	int decrement 		= jmax((int)1, historySize / 60);

	if(size == 0 || end == 0)
		return;

	// do in reverse due to transparency
    for (int col = end - 1; col > start + decrement; col -= decrement) {
        alphaA 		= (historySize - std::abs(playbackIndex - col)) / float(historySize);
		alphaA 		= alphaA * alphaA * 0.89f + 0.11f;
		blueAlpha 	= (historySize - 0.6f * std::abs(playbackIndex - col)) / float(historySize);
		blueAlpha 	= blueAlpha * blueAlpha * 0.89f + 0.11f;

        glColor4f(0.8f * alphaA, 0.9f * alphaA, blueAlpha, alphaA);

		ScopedElement gl(GL_LINE_STRIP);

		for(int i = 0; i < columns[col].size() - 1; ++i)
		{
			glVertex2f(sx(ramp[i]), sy(columns[col][i]));
			glVertex2f(sx(ramp[i + 1]), sy(columns[col][i]));
		}
	}

	gfx->checkErrors();
}

int Spectrum2D::getLayerScratchChannel() {
    return spectrum3D->getLayerScratchChannel();
}

void Spectrum2D::createScales() {
    std::cout << "creating scales for Spectrum2D\n";

	vector<Rectangle<float> > newScales;

	scalesImage = Image(Image::ARGB, 1024, 16, true);
	Graphics g(scalesImage);

	MiscGraphics& mg = getObj(MiscGraphics);
	int fontScale 	 = getSetting(PointSizeScale);
	Font& font 		 = *mg.getAppropriateFont(fontScale);
	float alpha 	 = fontScale == 1 ? 0.35f : 0.6f;

	g.setFont(font);

	int position = 0;

    for (int i = 0; i < decibelLines.size(); ++i) {
        int oldPos 	 = position;
		float absAmp = 2 * Arithmetic::invLogMapping(getConstant(FFTLogTensionAmp) * IPP_2PI, decibelLines[i]);
		String text  = String(roundToInt(NumberUtils::toDecibels(absAmp)));
		int width 	 = font.getStringWidth(text) + 1;

		mg.drawShadowedText(g, text, position + 1, font.getHeight(), font, alpha);
		newScales.push_back(Rectangle<float>(position, 0, width, font.getHeight()));

		position += width + 2;
	}

	int midiKey = getObj(MorphPanel).getCurrentMidiKey();

    if (midiKey >= 0) {
        Buffer<float> ramp = getObj(LogRegions).getRegion(midiKey);

        int rampIndex = 1;
        while (rampIndex < ramp.size() / 2) {
            int val = rampIndex * 2;
			String text;
			if(val > 1000) {
				text = String(val / 1000) + "k";
			} else {
				text = String(val);
			}

			int width = font.getStringWidth(text) + 1;
			mg.drawShadowedText(g, text, position + 1, font.getHeight(), font, alpha);
			newScales.push_back(Rectangle<float>(position, 0, width, font.getHeight()));

			position += width + 2;
			rampIndex *= 2;
		}
	}

	scales = newScales;
}


void Spectrum2D::drawScales() {
    if (getWidth() == 0 || getHeight() == 0)
        return;

    if (getSetting(MagnitudeDrawMode)) {
        float fontOffset = 3 - getSetting(PointSizeScale) * 1.5f;
		gfx->setCurrentColour(Color(1));

		int count = 0;
		for (int i = 0; i < (int) decibelLines.size(); ++i) {
            Rectangle<float>& rect = scales[i];
            float y = sy(decibelLines[i]);

            if (y < getHeight() - 10) {
                scalesTex->rect = Rectangle<float>(getWidth() - rect.getWidth() - 2, y, rect.getWidth(), rect.getHeight());
				gfx->drawSubTexture(scalesTex, rect);
			}

			++count;
		}

        const vector <Column>& columns = getObj(VisualDsp).getFreqColumns();
        int index = (columns.size() - 1) * getObj(PlaybackPanel).getProgress();

        if (index < columns.size()) {
            const Column& column = columns[index];

            if (column.midiKey > 0) {
                Buffer<float> ramp = getObj(LogRegions).getRegion(column.midiKey);

                int rampIndex = 1;

                while (rampIndex * 2 < ramp.size() && isPositiveAndBelow(count, (int) scales.size())) {
					Rectangle<float>& rect = scales[count];
					float x = sx(ramp[rampIndex * 2]) - rect.getWidth() - 2;

					scalesTex->rect = Rectangle<float>(x, fontOffset, rect.getWidth(), rect.getHeight());
					gfx->drawSubTexture(scalesTex, rect);

					++count;
					rampIndex *= 2;
				}
			}
		}
	}

	gfx->checkErrors();
}

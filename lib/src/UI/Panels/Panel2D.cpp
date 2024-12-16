#include "CommonGfx.h"
#include "Panel2D.h"

#include <Definitions.h>
#include <memory>

#include "OpenGLPanel.h"
#include "Panel3D.h"
#include "Texture.h"
#include "../../App/SingletonRepo.h"
#include "../../Array/Buffer.h"
#include "../../Curve/Intercept.h"
#include "../../Curve/MeshRasterizer.h"
#include "../../Inter/Interactor.h"
#include "../../Util/Arithmetic.h"
#include "../../Util/Geometry.h"
#include "../../UI/Layout/BoundWrapper.h"

Panel2D::Panel2D(SingletonRepo* repo,
				 const String& name,
				 bool curveIsBipolar,
				 bool haveVertZoom) :
		SingletonAccessor	(repo, name)
	,	Panel				(repo, name)
	,	curveIsBipolar		(curveIsBipolar)
	,	cyclicLines			(false) {
	alwaysDrawDepthLines = true;

	colorA 		= Color(0.6f, 0.6f, 0.6f);
	colorB 		= Color(0.5f, 0.5f, 0.5f, 0.7f);
	openGL		= std::make_unique<OpenGLPanel>(repo, this);
	wrapper		= std::make_unique<BoundWrapper>(openGL.get());
	zoomPanel 	= std::make_unique<ZoomPanel>(repo, ZoomContext(this, wrapper.get(), true, true));
	zoomPanel->panelComponentChanged(openGL.get());
	zoomPanel->addListener(this);
}

void Panel2D::contractToRange(bool includeX) {
	RasterizerData& data = interactor->getRasterizer()->getRastData();
	ScopedLock dataLock(data.lock);

	zoomPanel->contractToRange(data.waveY);
}

void Panel2D::drawCurvesAndSurfaces() {
    if (!shouldDrawCurve()) {
	    return;
    }

	Range<float> xLimit = interactor->vertexLimits[interactor->dims.x];
	bool extendsX 		= xLimit.getLength() > 1.f;
	bool reduceAlpha 	= ! isMeshEnabled();
	Color colourA 		= colorA.withAlpha(reduceAlpha ? 0.55f : 1.f);
	Color colourB 		= colorB.withAlpha(reduceAlpha ? 0.55f : 1.f);
	RasterizerData& data = interactor->getRasterizer()->getRastData();

	Buffer<float> a;

	{
		Buffer<float> waveY;
		Buffer<float> waveX;
		ScopedLock sl(data.lock);

		waveX = data.waveX;
		waveY = data.waveY;

		if(waveX.size() < 2) {
			return;
		}

		int istart 	= extendsX ? Arithmetic::binarySearch(xLimit.getStart(), waveX) : data.zeroIndex;
		int iend 	= extendsX ? Arithmetic::binarySearch(xLimit.getEnd(), waveX) : jmin((int) waveX.size() - 1, data.oneIndex + 4);

		if(istart > 4) {
			istart -= 4;
		}

		NumberUtils::constrain(istart, 0, iend);

		waveX = waveX.section(istart, iend - istart);
		waveY = waveY.section(istart, iend - istart);

		int size = waveX.size();
		prepareBuffers(size, size);

		xy.copyFrom(waveX, waveY);
		a = cBuffer.withSize(size);
	}

	prepareAlpha	(xy.y, a, colourA.alpha());
	applyScaleX		(xy.x);
	applyScaleY		(xy.y);
	drawCurvesFrom	(xy, a, colourA, colourB);
}

void Panel2D::drawCurvesFrom(BufferXY& xy, Buffer<float> alpha,
							 const Color& colourA, const Color& colourB) {
	float baseAlpha			= colourA.alpha() * 0.2f;
	float baseY 			= sy(curveIsBipolar ? 0.5f : 0.f);
	bool samplesAreBipolar 	= false;

	Color sectionClr = colourA;
	bool isLast = false;
	int size 	= xy.size();

	ColorPos curr;
	vector<ColorPos> positions;
	positions.reserve(size + 10);
	
	int i = 0;
	while(i < size - 1 && xy.x[i + 1] < 0) {
		++i;
	}

	sectionClr = xy.y[i] < baseY ? colourA : colourB; // after scaling graphic coords flipped
	curr.update(xy.x[i] - 5, xy.y[i], sectionClr.withAlpha(alpha.front()));
	positions.push_back(curr);

    for (; i < size; ++i) {
        if (i > 0 && xy.x[i - 1] > comp->getWidth())
            continue;

		isLast = i == size - 1;
		samplesAreBipolar = curveIsBipolar && ! isLast && (xy.y[i] - baseY) * (xy.y[i + 1] - baseY) < 0;

		curr.update(xy[i], sectionClr.withAlpha(alpha[i]));
		positions.push_back(curr);

		//zero crossing
        if (samplesAreBipolar) {
			float m 	= (xy.y[i + 1] - xy.y[i]) / (xy.x[i + 1] - xy.x[i]);
			float icptX = (m * xy.x[i] - xy.y[i] + baseY) / m;

			curr.update(icptX, baseY, sectionClr.withAlpha(alpha[i]));
			positions.push_back(curr);

			sectionClr = xy.y[i + 1] < baseY ? colourA : colourB;
			curr.update(icptX + 0.001f, baseY, sectionClr.withAlpha(alpha[i]));
			positions.push_back(curr);
		}
	}

	gfx->setCurrentLineWidth(interactor->mouseFlag(WithinReshapeThresh) ? 2.f : 1.f);
	gfx->fillAndOutlineColoured(positions, baseY, baseAlpha, true, true);
	gfx->setCurrentLineWidth(1.f);
}

void Panel2D::drawInterceptLines() {
	{
		RasterizerData& rastData = interactor->getRasterizer()->getRastData();
		ScopedLock sl(rastData.lock);
		const vector<Intercept>& intercepts = rastData.intercepts;

		if(intercepts.empty()) {
			return;
		}

		int size = intercepts.size() + (cyclicLines ? 2 : 0);

		prepareBuffers(size);

        int start = 0;
        if (cyclicLines) {
            xy.set(0, intercepts.back().x - 1, intercepts.back().y);
            ++start;
        }

        for (int i = 0; i < (int) intercepts.size(); ++i) {
            const Intercept& icpt = intercepts[i];
            xy.set(i + start, icpt);
        }

        if (cyclicLines) {
            xy.set(size - 1, intercepts.front().x + 1, intercepts.front().y);
		}
	}

	gfx->setCurrentLineWidth(1.f);
	gfx->setCurrentColour(0.2f, 0.2f, 0.2f, 0.9f);
	gfx->enableSmoothing();
	gfx->drawLineStrip(xy, true, true);
}

void Panel2D::highlightCurrentIntercept()
{
	Vertex2 point;

	int icptIdx = interactor->state.currentIcpt;
	const int freeIdx = interactor->state.currentFreeVert;

	if(icptIdx == -1 && freeIdx == -1)
		return;

	bool useFreeVert = icptIdx == -1;

    if (useFreeVert) {
		ScopedLock sl(interactor->getLock());
		vector<DepthVert>& verts = interactor->depthVerts;

		if(! isPositiveAndBelow(freeIdx, (int) verts.size()))
			return;

		point.x = verts[freeIdx].x;
		point.y = verts[freeIdx].y;
    } else {
		RasterizerData& data = interactor->getRasterizer()->getRastData();
		ScopedLock dataLock(data.lock);

		const vector<Intercept>& icpts = data.intercepts;

		if(! isPositiveAndBelow(icptIdx, (int) icpts.size())) {
			return;
		}

		if(interactor->dims.numHidden() > 0 && icpts[icptIdx].cube == nullptr && icptIdx > 0) {
			--icptIdx;
		}

		point.x = icpts[icptIdx].x;
		point.y = icpts[icptIdx].y;
	}

	gfx->setCurrentColour(1.0f, 0.8f, 0.0f);
	gfx->drawPoint(vertexHighlightRadius, point, true);
}

ostream& operator<<(ostream& stream, const Vertex* vert) {
	stream << "vertex: " << vert->values[0] << ", "
		   << vert->values[1] << ", " << vert->values[2] << ", "
		   << vert->values[3] << ", " << vert->values[4];

	return stream;
}

void Panel2D::prepareAlpha(const Buffer<float>& y, Buffer<float> alpha, float baseAlpha) {
	alpha.mul(y, 0.25f).abs().subCRev(0.5f).mul(baseAlpha);
}

void Panel2D::drawDepthLinesAndVerts() {
    RasterizerData& data = interactor->getRasterizer()->getRastData();

	if(data.colorPoints.empty())
		return;

	vector<ColorPoint> points;

	{
		ScopedLock sl(data.lock);
		points = data.colorPoints;
	}

	Color clr;
	int dim;
	float offsets[] 	= { 0, 0, 0, -0.5f, 0.5f };
	int scratchChannel 	= getLayerScratchChannel();

	gfx->setCurrentLineWidth(1.f);

	vector<ColorPoint> finalPoints;

	typedef vector<ColorPoint>::iterator ColorIter;

	for (auto& point : points) {
		dim = point.num;

		clr = pointColours[dim];

		// translate these points to icpt's point
		Vertex2 curr (sx(point.before.x) + offsets[dim], sy(point.before.y));
		Vertex2 next (sx(point.mid.x) + offsets[dim], sy(point.mid.y));
		Vertex2 next2(sx(point.after.x)  + offsets[dim], sy(point.after.y));

		Color c = clr.withAlpha(0.5f);

		gfx->drawLine(curr, next, c, clr);
		gfx->drawLine(next, next2,clr, c);
	}

	for (auto& p : points) {
		gfx->setCurrentColour(pointColours[p.num]);
		gfx->drawPoint(vertexWhiteRadius, p.before, true);
		gfx->drawPoint(vertexWhiteRadius, p.after, true);
	}
}

void Panel2D::drawDeformerTags() {
    RasterizerData& data = interactor->getRasterizer()->getRastData();
	vector<Curve>& curves = data.curves;

	Color colors[Vertex::numElements];
	colors[Vertex::Time]  = Color(0.6f, 0.6f, 0.6f, 1.f);
	colors[Vertex::Phase] = Color(0.65f, 0.6f, 0.4f, 1.f);
	colors[Vertex::Amp]   = Color(0.65f, 0.7f, 0.4f, 1.f);
	colors[Vertex::Red]   = Color(0.6f, 0.3f, 0.3f, 1.f);
	colors[Vertex::Blue]  = Color(0.3f, 0.4f, 0.65f, 1.f);
	colors[Vertex::Curve] = Color(0.7f, 0.5f, 0.8f, 1.f);

	ScopedLock sl2(data.lock);

	gfx->setCurrentColour(Color(1));

	if(dfrmTags.empty()) {
		return;
	}

	float h = dfrmTags.front().getHeight();

	for (auto& curve : curves) {
        Intercept& icpt = curve.b;

        if (VertCube* cube = icpt.cube) {
            if (!cube->isDeformed()) {
	            continue;
            }

			Vertex2 b(sx(curve.b.x), sy(curve.b.y));
			Vertex2 d(sx(curve.tp.d.x), sy(curve.tp.d.y));
			Vertex2 e(Geometry::getSpacedVertex(1.5f * h, d, b));

			int cumeWidth = 0;
			int numTags = 0;
            for (int j = 0; j < Vertex::numElements; ++j) {
	            int chan = cube->deformerAt(j);

                if (isPositiveAndBelow(chan, (int) dfrmTags.size())) {
                    Rectangle<float> rect = dfrmTags[chan];

					float x 	  = e.x + cumeWidth - 2;
					float y 	  = jmin(getHeight() - rect.getHeight(), e.y - rect.getHeight() / 2 + (numTags & 1 ? 1 : 0)); //  + 5
					dfrmTex->rect = Rectangle<float>((float) roundToInt(x), (float) roundToInt(y), rect.getWidth(), rect.getHeight());

					gfx->setCurrentColour(colors[j]);
					gfx->drawSubTexture(dfrmTex, rect);

					cumeWidth += rect.getWidth();
					++numTags;
				}
			}
		}
	}
}

bool Panel2D::isMeshEnabled() {
    return interactor->isCurrentMeshActive();
}

void Panel2D::zoomUpdated(int updateSource) {
    if (updateSource == interactor->getUpdateSource()) {
    } else {
        Interactor* opposite = interactor->getOppositeInteractor();

        if (opposite != nullptr) {
            if (updateSource == opposite->getUpdateSource()) {
                auto* panel3D = dynamic_cast<Panel3D*>(opposite->panel.get());

                if (panel3D != nullptr) {
					ZoomRect& rectSrc = panel3D->getZoomPanel()->rect;
					ZoomRect& rectDst = getZoomPanel()->rect;

					rectDst.h = rectSrc.w;
					rectDst.y = (1 - rectDst.h) - rectSrc.x;

					panel3D->updateBackground(false);
					panel3D->bakeTexturesNextRepaint();
					panel3D->repaint();
				}
			}

			repaint();
		}
	}
}

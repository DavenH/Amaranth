#include "CommonGfx.h"
#include "Panel3D.h"
#include "Texture.h"
#include "ZoomPanel.h"
#include "OpenGLPanel3D.h"
#include "../../App/MeshLibrary.h"
#include "../../App/Settings.h"
#include "../../App/SingletonRepo.h"
#include "../../Curve/DepthVert.h"
#include "../../Inter/Interactor3D.h"
#include "../../UI/Layout/BoundWrapper.h"
#include "../../Util/CommonEnums.h"
#include "../../Util/Geometry.h"
#include "../../Util/LogRegions.h"
#include "../../Util/ScopedFunction.h"


Panel3D::Panel3D(
    SingletonRepo* repo,
    const String& name,
    DataRetriever* retriever,
    int updateSource,
    bool isTransparent,
    bool haveHorzZoom
) :
        Panel				(repo, name, isTransparent)
    ,	SingletonAccessor	(repo, name)
    ,	volumeScale			(1.f)
    ,	volumeTrans			(0.5f)
    ,	haveLogarithmicY	(false)
    ,	updateSource	    (updateSource)
    ,	useVertices			(useVertices)
    ,	haveHorzZoom	    (haveHorzZoom)
    ,	dataRetriever		(retriever)
{
}

Panel3D::~Panel3D() {
    clrIndicesA	.clear();
    clrIndicesB	.clear();
    downsampAcc	.clear();
    downsampBuf	.clear();
    scaledX 	.clear();
    scaledY 	.clear();
    vertices 	.clear();
    fColours 	.clear();
    fDestColours.clear();
    texColours 	.clear();
    colours 	.clear();
}

void Panel3D::init() {
    Panel::init();

    openGL 		= std::make_unique<OpenGLPanel3D>(repo, this, dataRetriever);
    wrapper 	= std::make_unique<BoundWrapper>(openGL.get());
    zoomPanel 	= std::make_unique<ZoomPanel>(repo, ZoomContext(this, wrapper.get(), haveHorzZoom, true));
    zoomPanel->addListener(this);

    drawLinesAfterFill = true;
}

void Panel3D::bakeTextures() {
    shouldBakeTextures = false;

    ScopedFunction sf(
            renderer.get(),
            &Renderer::textureBakeBeginning,
            &Renderer::textureBakeFinished);
    drawSurface();
}

void Panel3D::drawDepthLinesAndVerts() {
    // just don't draw them
}

void Panel3D::drawInterceptLines() {
    auto* itr3D = dynamic_cast<Interactor3D*>(interactor.get());
    vector<SimpleIcpt> interceptPairs;

    {
        ScopedLock sl(itr3D->getLock());
        interceptPairs = itr3D->getInterceptPairs();
    }

    if(interceptPairs.empty()) {
        return;
    }

    bool haveSpeed 		= false;
    int scratchChannel 	= getLayerScratchChannel();
    int dim = getSetting(CurrentMorphAxis);

    if (scratchChannel != CommonEnums::Null && isScratchApplicable()) {
        if (MeshLibrary::Properties* props = getLayerProps(GroupScratch, scratchChannel)) {
            haveSpeed = props->active;
        }
    }

    Vertex* currVert = nullptr;

    if(Interactor* opp 	= interactor->getOppositeInteractor())
        currVert = opp->state.currentVertex;

    float x1, y1, x2, y2;

    for (int i = 0; i < (int) interceptPairs.size() - 1; i += 2) {
        VertCube* cube = interceptPairs[i].parent;
        const Vertex2& first = Vertex2(interceptPairs[i]);
        Vertex2 second = Vertex2(interceptPairs[i + 1]);

        if (createLinePath(first, second, cube, dim, haveSpeed)) {
            xy.x = xBuffer.withSize(linestripRes);
            xy.y = yBuffer.withSize(linestripRes);

            Vertex2 realFirst(xy.front());
            Vertex2 realLast(xy.back());

            realLast.x *= 0.9999f;
            second.x *= 0.9999f;

            applyScale(xy);

            gfx->enableSmoothing();

            bool revertWidth = false;
            if (currVert != nullptr && currVert->isOwnedBy(cube)) {
                gfx->setCurrentLineWidth(2.f);
                revertWidth = true;
            }

            gfx->setCurrentColour(0.1f, 0.1f, 0.3f, 0.6f);
            gfx->drawLineStrip(xy, true, false);

            xy.x.add(0.5f);
            xy.y.add(0.5f);

            gfx->setCurrentColour(0.7f, 0.7f, 1.0f, 0.8f);
            gfx->drawLineStrip(xy, true, false);
            gfx->setCurrentColour(0.7f, 0.7f, 0.7f, 0.5f);

            if(revertWidth) {
                gfx->setCurrentLineWidth(1.f);
            }

            gfx->disableSmoothing();
            gfx->drawLine(first, realFirst, true);
            gfx->drawLine(realLast, second, true);
        } else {
            gfx->enableSmoothing();
            x1 = sx(first.x);
            x2 = sx(second.x);
            y1 = sy(first.y);
            y2 = sy(second.y);

            bool revertWidth = false;
            if(currVert != nullptr && currVert->isOwnedBy(cube)) {
                gfx->setCurrentLineWidth(2.f);
                revertWidth = true;
            }

            gfx->setCurrentColour(0.1f, 0.1f, 0.3f, 0.6f);
            gfx->drawLine(x1, y1 + 0.5f, x2, y2 + 0.5f, false);

            gfx->setCurrentColour(0.7f, 0.7f, 1.0f, 0.8f);
            gfx->drawLine(x1, y1, x2, y2, false);

            if(revertWidth)
                gfx->setCurrentLineWidth(1.f);
        }
    }
}

void Panel3D::drawInterceptsAndHighlightClosest() {
    // it's a bigger circle, so do it first
    highlightCurrentIntercept();

    auto* itr3D = dynamic_cast<Interactor3D*>(interactor.get());

    vector<SimpleIcpt> interceptPairs;

    {
        ScopedLock sl(itr3D->getLock());
        interceptPairs = itr3D->getInterceptPairs();
    }

    if(interceptPairs.empty()) {
        return;
    }

    int size = interceptPairs.size();

    prepareAndCopy(interceptPairs);
    drawScaledInterceptPoints(size);
}

void Panel3D::drawAxe() {
    Vertex2 curr = interactor->state.currentMouse;
    float length = interactor->realValue(PencilRadius);

    gfx->setCurrentColour(1, 1, 1);
    gfx->drawLine(Vertex2(curr.x, curr.y - length), Vertex2(curr.x, curr.y + length));
}

/*
 * Code path:
 *
 * with vertices: values -> colour lookup -> pixels
 * without verts: values -> lookup -> float colours -> resample to tex height -> resamp float colours -> pixels
 */
void Panel3D::drawLogSurface(const vector<Column>& grid) {
    Buffer<Ipp8u> pxBuf 	= gradient.getPixels();
    Buffer<float> floatBuf = gradient.getFloatPixels();

    for (int i = 0; i < draw.sizeX; ++i) {
        draw.colSourceSizeY = draw.sizeY = grid[i].size();

        adjustLogColumnAndScale(grid[i].midiKey, i == 0);
        grid[i].copyTo(downsampAcc);

        doColumnDraw(pxBuf, floatBuf, i);
    }
}

void Panel3D::drawLinSurface(const vector<Column>& grid) {
    downsampBuf.resize(jmin(maxHorzLines, draw.colSourceSizeY));

    Buffer<Ipp8u> pxBuf 	= gradient.getPixels();
    Buffer<float> floatBuf 	= gradient.getFloatPixels();

    for (int i = 0; i < draw.sizeX; ++i) {
        const Buffer<float>& gridBuf = grid[i];
        draw.colSourceSizeY = draw.sizeY = gridBuf.size();

        if(draw.sizeY <= maxHorzLines) {
            gridBuf.copyTo(downsampAcc);
        } else {
            // nb this can adjust size-y
            downsampleColumn(gridBuf);
        }

        if (draw.lastSizeY != draw.sizeY) {
            Buffer<float> y = scaledY.withSize(draw.sizeY);

            applyScaleY(y.ramp());
        }

        draw.lastSizeY = draw.sizeY;

        doColumnDraw(pxBuf, floatBuf, i);
    }
}

void Panel3D::doColumnDraw(Buffer<Ipp8u> pxBuf, Buffer<float> grd32f, int i) {
    resizeArrays();
    setColumnColourIndices();

    Buffer fdest = fDestColours;

    if (!useVertices) {
        doColourLookup32f(grd32f, fColours);
        resampleColours(fColours, fdest);

        fdest.threshGT(255.f / 256.f);
        ippsConvert_32f8u_Sfs(fdest, texColours, draw.stride * draw.texHeight, ippRndZero, -8);
    }

    if (i > 0 && useVertices) {
        doColourLookup8u(pxBuf, colours);
        setVertices(i, vertices);
    }

    if(i > 0 || ! useVertices) {
        renderer->drawSurfaceColumn(i);
    }

  #ifdef JUCE_DEBUG
    gfx->checkErrors();
  #endif
}

void Panel3D::adjustLogColumnAndScale(int key, bool firstColumn) {
    if (draw.adjustColumns || firstColumn) {
        draw.currKey = key;

        if (draw.currKey != draw.lastKey || firstColumn) {
            Buffer<float> ramp = getObj(LogRegions).getRegion(draw.currKey);
            jassert(scaledY.size() >= ramp.size());

            draw.ramp = ramp;
            ramp.copyTo(scaledY);
            applyScaleY(scaledY.withSize(draw.sizeY));
        }

        draw.lastKey = draw.currKey;
    }
}

void Panel3D::downsampleColumn(const Buffer<float>& column) {
    int downsampleFactor    = draw.colSourceSizeY / maxHorzLines;
    draw.sizeY 				= maxHorzLines;

    jassert(downsampAcc.size() >= draw.sizeY);

    Buffer<float> downsampAcc(downsampAcc.withSize(draw.sizeY));
    Buffer downsampBuff(downsampBuf.withSize(draw.sizeY));

    downsampAcc.zero();

    int buffsToMerge = draw.reduced ? 1 : downsampleFactor;
    for (int k = 0; k < buffsToMerge; ++k) {
        int phase = k;

        downsampBuff.downsampleFrom(column, downsampleFactor, phase);
        downsampAcc.add(downsampBuff);
    }

    if(buffsToMerge > 1) {
        downsampAcc.mul(1 / float(buffsToMerge));
    }
}

void Panel3D::setColumnColourIndices() {
    ippsCopy_16s(clrIndicesB, clrIndicesA, draw.sizeY);

    Buffer<float> downsampAcc(downsampAcc.withSize(draw.sizeY));
    downsampAcc.mul(volumeScale).add(volumeTrans);

    ippsConvert_32f16s_Sfs(downsampAcc, clrIndicesB, draw.sizeY, ippRndZero, -9);
    ippsThreshold_16s_I(clrIndicesB, draw.sizeY, gradientWidth - 1, ippCmpGreater);
    ippsThreshold_16s_I(clrIndicesB, draw.sizeY, 0, ippCmpLess);
    ippsMulC_16s_I(draw.stride, clrIndicesB, draw.sizeY);
}

void Panel3D::setVertices(int column, Buffer<float> vertices) const {
    jassert(4 * (draw.sizeY + 1) <= vertices.size());

    Ipp32f* vertexPtr = vertices;
    Ipp32f* x0 = scaledX.get() + column - 1;
    Ipp32f* y0 = scaledY.get();
    Ipp32f* x1 = x0 + 1;

    // fill in bottom
    *vertexPtr++ = *x0;
    *vertexPtr++ = *y0 + 15; // good lord what is 15 bytes?
    *vertexPtr++ = *x1;
    *vertexPtr++ = *y0 + 15;

    for (int j = 0; j < draw.sizeY; ++j) {
        *vertexPtr++ = *x0;
        *vertexPtr++ = *y0;
        *vertexPtr++ = *x1;
        *vertexPtr++ = *y0++;
    }
}

void Panel3D::doColourLookup8u(Buffer<Ipp8u> grd, Buffer<Ipp8u> colors) {
    Ipp8u* grd8u = grd;
    Ipp8u* colorPtr = colors;
    int bytes = draw.stride;

    jassert(2 * bytes * (draw.sizeY + 1) <= colors.size());

    ippsCopy_8u(grd8u + clrIndicesA[0], colorPtr, bytes);
    colorPtr += bytes;
    ippsCopy_8u(grd8u + clrIndicesB[0], colorPtr, bytes);
    colorPtr += bytes;

    for(int j = 0; j < draw.sizeY; ++j) {
        ippsCopy_8u(grd8u + clrIndicesA[j], colorPtr, bytes);
        colorPtr += bytes;

        ippsCopy_8u(grd8u + clrIndicesB[j], colorPtr, bytes);
        colorPtr += bytes;
    }
}

void Panel3D::doColourLookup32f(Buffer<float> grd, Buffer<float> colors) {
    Ipp32f* grd32f = grd;
    Ipp32f* colorPtr = colors;
    int bytes = draw.stride;

    jassert(draw.sizeY * bytes <= colors.size());

    for (int j = 0; j < draw.sizeY; ++j) {
        ippsCopy_32f(grd32f + clrIndicesB[j], colorPtr, bytes);
        colorPtr += bytes;
    }
}

void Panel3D::resizeArrays() {
    if (useVertices) {
        colours.resize(vertsPerQuad * (draw.colSourceSizeY + 1) * draw.stride);
        vertices.resize(vertsPerQuad * coordsPerVert * (draw.colSourceSizeY + 1));
    } else {
        fColours	.resize(draw.sizeY * draw.stride);
        fDestColours.resize(draw.texHeight * draw.stride);
        texColours	.resize(draw.texHeight * draw.stride);
    }
}

/*
 * reduce log scale y:
 *
 * while pos < height
 * 		val = 0
 * 		while x[c] < pos
 * 			val += y[c]
 * 			c++
 * 		pos++
 */
void Panel3D::resampleColours(Buffer<float> srcColors, Buffer<float> dstColorBuf) {
    Ipp32f* colorPtr 	= srcColors;
    Ipp32f* dstColors 	= dstColorBuf;
    Ipp32f* startClrPtr = colorPtr;
    Ipp32f* startDstPtr = dstColors;

    jassert(draw.stride * draw.texHeight <= dstColorBuf.size());

    int trunc, truncStride;
    float phase = 0;
    int subsampleIdx = draw.texHeight;
    int i = 0;

    // scaled y is logarithmic ramp
    if (haveLogarithmicY) {
        int srcIdx 		= 1;
        int idxStride 	= srcIdx * draw.stride;

        float portion, prev;
        float floatIdx 	= 0;
        float invHeight = 1 / float(draw.texHeight);
        int rampSize 	= draw.ramp.size();

        Ipp32f* rampPtr = draw.ramp.get();
        Ipp32f* endClrPtr = startClrPtr + draw.stride * rampSize;

        while ((floatIdx = i * invHeight) < draw.ramp.front()) {
            ippsCopy_32f(colorPtr, dstColors, draw.stride);
            dstColors += draw.stride;
            ++i;
        }

        while (i < subsampleIdx) {
            floatIdx = i * invHeight;

            while (rampPtr[srcIdx] < floatIdx && srcIdx < rampSize) {
                colorPtr += draw.stride;
                ++srcIdx;
            }

            prev 	= rampPtr[srcIdx - 1];
            portion = (floatIdx - prev) / (rampPtr[srcIdx] - prev);

            jassert(colorPtr + 3 < endClrPtr);

            ippsMulC_32f		(colorPtr, 				 1 - portion, 	dstColors, 4);
            ippsAddProductC_32f	(colorPtr + draw.stride, portion, 		dstColors, 4);

            dstColors += draw.stride;
            ++i;
        }

        idxStride = draw.stride * (draw.ramp.size() - 1);
        int lastDst = draw.stride * (subsampleIdx - 1);

        ippsCopy_32f(startClrPtr + idxStride, startDstPtr + lastDst, draw.stride);
    } else {
        // scaled y is linear ramp of values
        float remainder;
        double sourceToDestRatio = (double) draw.sizeY / draw.texHeight;

        while (i < subsampleIdx - 1) {
            trunc = (int) phase;
            remainder = phase - trunc;
            truncStride = draw.stride * trunc;

            ippsMulC_32f		(colorPtr + truncStride, 				1 - remainder, 	dstColors + draw.stride * i, draw.stride);
            ippsAddProductC_32f	(colorPtr + truncStride + draw.stride, 	remainder, 		dstColors + draw.stride * i, draw.stride);

            phase += sourceToDestRatio;

            ++i;
        }

        jassert(draw.stride * (draw.sizeY - 1) + draw.stride - 1 < srcColors.size());

        ippsCopy_32f(colorPtr + draw.stride * (draw.sizeY - 1), dstColors + draw.stride * (subsampleIdx - 1), draw.stride);
    }
}

vector<Color>& Panel3D::getGradientColours() {
    return gradient.getColours();
}

void Panel3D::drawSurface() {
    if(! renderer->shouldDrawGrid()) {
        return;
    }

    const vector<Column>& grid = renderer->getColumns();

    if(grid.empty()) {
        return;
    }

    CriticalSection dummy;
    CriticalSection& arrayLock = renderer->getGridLock();

    ScopedLock sl(arrayLock);

    draw.lastSizeY 		= 0;
    draw.lastKey 		= 0;
    draw.sizeX 			= grid.size();
    draw.sizeY 			= grid.front().size();
    draw.currKey 		= grid.front().midiKey;
    draw.colSourceSizeY = draw.sizeY;
    draw.downsampSize 	= draw.sizeY;
    draw.stride 		= isTransparent ? 4 : 3;
    draw.texHeight 		= comp->getHeight() - 2 * vertPadding;
    draw.reduced		= renderer->isDetailReduced();
    draw.adjustColumns 	= renderer->willAdjustColumns();
    draw.ramp			= Buffer<float>();

    downsampAcc.resize(draw.colSourceSizeY);
    clrIndicesA.resize(draw.colSourceSizeY);
    clrIndicesB.resize(draw.colSourceSizeY);
    scaledY.resize(draw.colSourceSizeY);
    scaledX.resize(draw.sizeX);

    float slope = (grid.back().x - grid.front().x) / float(draw.sizeX - 1);

    scaledX.ramp(0, slope);
    applyScaleX(scaledX);

    ScopedFunction sf(
        renderer.get(),
        &Renderer::gridDrawBeginning,
        &Renderer::gridDrawFinished);

    haveLogarithmicY ?
        drawLogSurface(grid) :
        drawLinSurface(grid);
}

void Panel3D::postVertsDraw() {
    PanelState& state = interactor->state;

    if (getSetting(Tool) == Tools::Axe && mouseFlag(MouseOver)) {
        drawAxe();
    }

    renderer->postVertsDraw();
}

void Panel3D::highlightCurrentIntercept() {
    Vertex2 point;

    {
        ScopedLock sl(interactor->getLock());

        const int icptIdx = interactor->state.currentIcpt;
        const int freeIdx = interactor->state.currentFreeVert;

        if(icptIdx == -1 && freeIdx == -1) {
            return;
        }

        bool useFreeVert = icptIdx == -1;

        if(useFreeVert) {
            vector<DepthVert>& verts = interactor->depthVerts;

            if(! isPositiveAndBelow(freeIdx, (int) verts.size())) {
                return;
            }

            point.x = verts[freeIdx].x;
            point.y = verts[freeIdx].y;
        } else {
            auto* itr3D = dynamic_cast<Interactor3D*>(interactor.get());
            const vector<SimpleIcpt>& interceptPairs = itr3D->getInterceptPairs();

            if(! isPositiveAndBelow(icptIdx, (int) interceptPairs.size())) {
                return;
            }

            point.x = interceptPairs[icptIdx].x;
            point.y = interceptPairs[icptIdx].y;
        }
    }

    gfx->setCurrentColour(1.f, 0.8f, 0.0f);
    gfx->drawPoint(vertexHighlightRadius, point, true);
}

void Panel3D::doExtraResized() {
    shouldBakeTextures = true;
}

void Panel3D::freeResources() {
    vertices	.clear();
    colours		.clear();
    fColours	.clear();
    fDestColours.clear();
    texColours	.clear();
}

void Panel3D::drawDeformerTags() {
    vector<SimpleIcpt> interceptPairs;

    {
        ScopedLock sl(interactor->getLock());
        interceptPairs = interactor3D->getInterceptPairs();
    }

    Color colors[Vertex::numElements];
    colors[Vertex::Time]  = Color(0.6f, 0.6f, 0.6f);
    colors[Vertex::Phase] = Color(0.65f, 0.6f, 0.4f);
    colors[Vertex::Amp]   = Color(0.65f, 0.7f, 0.4f);
    colors[Vertex::Red]   = Color(0.6f, 0.3f, 0.3f);
    colors[Vertex::Blue]  = Color(0.3f, 0.4f, 0.65f);
    colors[Vertex::Curve] = Color(0.7f, 0.5f, 0.8f);

    for (int i = 0; i < (int) interceptPairs.size() - 1; i += 2) {
        VertCube* cube = interceptPairs[i].parent;

        Vertex2 first = Vertex2(interceptPairs[i]);
        Vertex2 second = Vertex2(interceptPairs[i + 1]);

        if (cube != nullptr && cube->isDeformed()) {
            Vertex2 b(sx(first.x), 	sy(first.y));
            Vertex2 d(sx(second.x), sy(second.y));
            Vertex2 e(Geometry::getSpacedVertex(- 6, d, b));

            int cumeWidth = 0;
            int numTags = 0;

            for(int j = 0; j < Vertex::numElements; ++j) {
                int chan = cube->deformerAt(j);

                if (isPositiveAndBelow(chan, (int) dfrmTags.size())) {
                    Rectangle<float> rect = dfrmTags[chan];

                    float x = e.x + cumeWidth - 2;
                    float y = jmin(getHeight() - 16.f, e.y + 2.f + (numTags & 1 ? 1 : 0)); //  + 5
                    dfrmTex->rect = Rectangle(roundf(x), roundf(y), rect.getWidth(), rect.getHeight());

                    gfx->setCurrentColour(colors[j]);
                    gfx->drawSubTexture(dfrmTex, rect);

                    cumeWidth += rect.getWidth();
                    ++numTags;
                }
            }
        }
    }
}

void Panel3D::zoomUpdated(int updateSource) {
    if (updateSource == interactor->getUpdateSource()) {
    } else {
        Interactor* opposite = interactor->getOppositeInteractor();
        if (opposite != nullptr) {
            if (updateSource == opposite->getUpdateSource()) {
                ZoomRect& rectSrc = opposite->panel->getZoomPanel()->rect;
                ZoomRect& rectDst = getZoomPanel()->rect;

                rectDst.h = rectSrc.w;
                rectDst.y = (1 - rectDst.h) - rectSrc.x;

                 opposite->panel->repaint();
            }

            updateBackground(false);
        }

        bakeTexturesNextRepaint();
        repaint();
    }
}

void Panel3D::dragStarted() {
    getObj(Updater).update(updateSource, ReduceDetail);
}

void Panel3D::dragEnded() {
    getObj(Updater).update(updateSource, RestoreDetail);
}
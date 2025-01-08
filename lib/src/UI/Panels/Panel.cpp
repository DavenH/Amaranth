#include <utility>
#include "Panel.h"

#include <Definitions.h>
#include <App/AppConstants.h>

#include "CommonGfx.h"
#include "Texture.h"
#include "CursorHelper.h"
#include "../MiscGraphics.h"
#include "../../App/Settings.h"
#include "../../App/SingletonRepo.h"
#include "../../Binary/Images.h"
#include "../../Curve/IDeformer.h"
#include "../../Curve/MeshRasterizer.h"
#include "../../Curve/PathRepo.h"
#include "../../Curve/RasterizerData.h"
#include "../../Inter/Interactor.h"
#include "../../Obj/Color.h"
#include "../../Thread/LockTracer.h"
#include "../../Util/MicroTimer.h"
#include "../../Util/Util.h"

int panelCount = 0;

Panel::Panel(SingletonRepo* repo, const String& name, bool isTransparent) :
        SingletonAccessor       (repo, name)
    ,   isTransparent           (isTransparent)
    ,   alwaysDrawDepthLines    (false)
    ,   backgroundTempoSynced   (false)
    ,   backgroundTimeRelevant  (true)
    ,   deformApplicable        (true)
    ,   doesDrawMouseHint       (false)
    ,   drawLinesAfterFill      (false)
    ,   pendingDeformUpdate     (true)
    ,   pendingNameUpdate       (false)
    ,   pendingScaleUpdate      (false)
    ,   shouldBakeTextures      (true)
    ,   speedApplicable         (true)

    ,   numCornersOverlapped    (0)
    ,   vertPadding             (3)
    ,   paddingLeft             (3)
    ,   paddingRight            (3)
    ,   maxWidth                (0)
    ,   lastFrameTime           (0)

    ,   minorBrightness         (0.085f)
    ,   majorBrightness         (0.14f)

    ,   bgPaddingLeft           (0.f)
    ,   bgPaddingRight          (0.f)
    ,   bgPaddingTop            (0.f)
    ,   bgPaddingBttm           (0.f)

    ,   vertexWhiteRadius       (2.f)
    ,   vertexSelectedRadius    (3.f)
    ,   vertexBlackRadius       (5.f)
    ,   vertexHighlightRadius   (7.f)

    ,   xBuffer                 (512)
    ,   yBuffer                 (512)
    ,   cBuffer                 (512)
    ,   stripRamp               (linestripRes)
    ,   bgLinesMemory           (2 * maxMajorSize + 2 * maxMinorSize)

    ,   interactor              (nullptr)

    ,   nameTexA                (nullptr)
    ,   nameTexB                (nullptr)
    ,   dfrmTex                 (nullptr)
    ,   grabTex                 (nullptr)
    ,   scalesTex               (nullptr)

    ,   panelName               (name)
    ,   currentNameId           (NameTexture)
    ,   nameCornerPos           (-8, 5) {
    pointColours[Vertex::Time]  = Color(0.6f,   0.55f,  0.f,    0.9f);
    pointColours[Vertex::Red]   = Color(0.6f,   0.1f,   0.1f,   0.9f);
    pointColours[Vertex::Blue]  = Color(0.15f,  0.35f,  0.75f,  1.f);
    pointColours[Vertex::Phase] = Color(0.9f,   0.9f,   0.9f,   1.f);

    panelId        = panelCount++;
    grabImage      = PNGImageFormat::loadFrom(Images::grabtex_png, Images::grabtex_pngSize);

    vertMajorLines = bgLinesMemory.place(maxMajorSize);
    horzMajorLines = bgLinesMemory.place(maxMajorSize);
    vertMinorLines = bgLinesMemory.place(maxMinorSize);
    horzMinorLines = bgLinesMemory.place(maxMinorSize);
}

#ifdef JUCE_DEBUG
 #define CHECK_ERRORS gfx->checkErrors() ;
#else
 #define CHECK_ERRORS
#endif

Panel::~Panel() {
    gfx = nullptr;
}

void Panel::render() {
    // should only be held on mesh deletions
    ScopedLock sl(renderLock);

    gfx->initRender();
    handlePendingUpdates();

    PanelState& state = interactor->state;

    clear();
    drawBackground();

    if(shouldBakeTextures) {
        bakeTextures();
    }

    bool drawVerts = ! getSetting(ViewVertsOnlyOnHover) || mouseFlag(MouseOver);

    preDraw();

    if(! drawLinesAfterFill && drawVerts) {
        drawInterceptLines();
    }

    drawCurvesAndSurfaces();

    if(drawLinesAfterFill && drawVerts) {
        drawInterceptLines();
    }

    drawOutline();
    postCurveDraw();

    if (drawVerts) {
        drawDepthLinesAndVerts();
        drawInterceptsAndHighlightClosest();
        drawViewableVerts();
        highlightSelectedVerts();
    }

    postVertsDraw();

    if (mouseFlag(MouseOver) && getSetting(DrawScales)) {
        gfx->setCurrentColour(Color(1, 1, 1));
        gfx->drawTexture(currentNameId == NameTexture ? nameTexA : nameTexB);

        drawScales();
        drawDeformerTags();
    }

    if (mouseFlag(MouseOver)) {
        drawFinalSelection();
    }

    drawPencilPath();

    if(actionIs(BoxSelecting)) {
        drawSelectionRectangle();
    }

    CHECK_ERRORS
}

void Panel::constrainZoom() {
    ZoomRect& rect = zoomPanel->rect;
    NumberUtils::constrain<float>(rect.w, 0.001f, rect.xMaximum - rect.xMinimum);
    NumberUtils::constrain<float>(rect.h, 0.005f, 1);
    NumberUtils::constrain<float>(rect.x, interactor->vertexLimits[interactor->dims.x]);
    NumberUtils::constrain<float>(rect.y, 0, 1);

    if(rect.y + rect.h > 1) {
        rect.y = 1 - rect.h;
    }

    if(rect.x + rect.w > rect.xMaximum) {
        rect.x = rect.xMaximum - rect.w;
    }
}

void Panel::drawBackground(bool fillBackground) {
    drawBackground(comp->getBounds(), fillBackground);
}

void Panel::panelResized() {
    if (interactor) {
        interactor->resizeFinalBoxSelection();
    }

    updateNameTexturePos();
    updateBackground();
    doExtraResized();
}

void Panel::updateNameTexturePos() {
    if (nameTexA == nullptr) {
        return;
    }

    bool fromBottom = nameCornerPos.getY() < 0;
    int w = nameImage.getWidth();
    int h = nameImage.getHeight();

    Rectangle<int> bounds = comp->getLocalBounds();
    nameTexA->rect = (fromBottom ?  bounds.removeFromRight(w).removeFromBottom(h) :
                                    bounds.removeFromRight(w).removeFromTop(h)).toFloat();

    Point<float> offset = nameCornerPos;
    nameTexA->rect.translate(offset.getX(), offset.getY());
    nameTexB->rect = nameTexA->rect;
}

void Panel::updateBackground(bool onlyVerticalBackground) {
    ScopedLock sl(renderLock);

    int w = comp->getWidth();
    int h = comp->getHeight();

    int minorLineOrder = 64;
    int majorLineOrder = 8;

    ZoomRect& rect = zoomPanel->rect;

    float zoomLeft  = rect.x - bgPaddingLeft;
    float zoomRight = rect.x + rect.w - bgPaddingRight; //xMaximum;

    Vertex2 fineInc, crsInc;
    float xScale = rect.xMaximum - bgPaddingLeft - bgPaddingRight;
    float yScale = rect.yMaximum - bgPaddingTop - bgPaddingBttm;

    if (backgroundTimeRelevant) {
        float timeInc   = 1.f / NumberUtils::nextPower2(roundToInt(16 / rect.w));
        crsInc.x        = 4 * timeInc;
        fineInc.x       = timeInc;
    } else {
        fineInc.x       = xScale / float(minorLineOrder);
        crsInc.x        = xScale / float(majorLineOrder);
    }

    // fine
    {
        int startIdx    = jmax(0, (int) ceilf(zoomLeft / fineInc.x));
        int endIdx      = ceilf(zoomRight / fineInc.x);
        float offset    = bgPaddingLeft + startIdx * fineInc.x;
        int maxSize     = jmin(int(maxMinorSize), (int) ceilf(xScale / fineInc.x));
        int numLines    = jmin(maxSize, endIdx - startIdx) + 1;

        vertMinorLines.resize(numLines);
        vertMinorLines.ramp(offset, fineInc.x);
    }

    // coarse
    {
        int startIdx    = jmax(0, (int) ceilf(zoomLeft / crsInc.x));
        int endIdx      = ceilf(zoomRight / crsInc.x);
        float offset    = bgPaddingLeft + startIdx * crsInc.x;
        int maxSize     = jmin(int(maxMajorSize), (int) ceilf(xScale / crsInc.x));
        int numLines    = jmin(maxSize, endIdx - startIdx);

        vertMajorLines.resize(numLines);
        vertMajorLines.ramp(offset, crsInc.x);
    }

    float ratio = float(w * 0.8) / float(jmax(1, h));
    int reductRatio = jmax(1, roundToInt(ratio));

    if (!onlyVerticalBackground) {
        {
            int numLines = int(minorLineOrder / reductRatio);
            numLines     = jlimit(2, (int)maxMinorSize, NumberUtils::nextPower2(numLines));
            fineInc.y    = yScale / numLines;

            horzMinorLines.resize(numLines);
            horzMinorLines.ramp(bgPaddingBttm + fineInc.y, fineInc.y);
        }

        {
            int numLines = int(majorLineOrder / reductRatio);
            numLines     = jlimit(2, (int)maxMajorSize, NumberUtils::nextPower2(numLines));
            crsInc.y     = yScale / numLines;

            horzMajorLines.resize(numLines - 1);
            horzMajorLines.ramp(bgPaddingBttm + crsInc.y, crsInc.y);
        }
    } else {
        // nb: not nullify; want to keep pointer
        horzMinorLines.resize(0);
        horzMajorLines.resize(0);
    }

    pendingScaleUpdate = true;
}

void Panel::applyScale(BufferXY& buff) {
    applyScaleX(buff.x);
    applyScaleY(buff.y);
}

void Panel::applyScaleX(Buffer<Float32> array) {
    float sumX = -zoomPanel->rect.x;
    float multX = (comp->getWidth() - paddingLeft - paddingRight) / zoomPanel->rect.w;

    array.add(sumX).mul(multX).add(paddingLeft);
}

void Panel::applyScaleY(Buffer<Float32> array) {
    float multY = (comp->getHeight() - 2 * vertPadding) / zoomPanel->rect.h;
    float sumY = 1 - zoomPanel->rect.y;

    array.mul(-1).add(sumY).mul(multY).add(vertPadding);
}

void Panel::applyNoZoomScaleX(Buffer<Float32> array) {
    float multX = (comp->getWidth() - (paddingLeft + paddingRight));

    array.mul(multX).add(paddingLeft);
}

void Panel::drawViewableVerts() {
    if (interactor->depthVerts.empty()) {
        return;
    }

    int size = 0;

    // verts within our view cube
    {
        ScopedLock sl(interactor->getLock());
        const vector<DepthVert>& verts = interactor->depthVerts;

        size = verts.size();
        prepareAndCopy(verts);
    }

    gfx->scaleIfNecessary(true, xy);
    gfx->setCurrentColour(0, 0, 0);
    gfx->drawPoints(vertexBlackRadius, xy, false);
    gfx->setCurrentColour(1, 1, 1);
    gfx->drawPoints(vertexWhiteRadius, xy, false);
}

void Panel::highlightSelectedVerts() {
    int size = 0;
    {
        ScopedLock sl(interactor->getLock());

        vector<Vertex*>& selected = interactor->getSelected();

        if(selected.empty()) {
            return;
        }

        bool wrapsVerts = interactor->getRasterizer()->wrapsVertices();

        size = selected.size();
        prepareBuffers(size);

        for (int i = 0; i < size; ++i) {
            Vertex& vert = *selected[i];
            float vals[] = {vert.values[interactor->dims.x], vert.values[interactor->dims.y]};
            int dimArr[] = {interactor->dims.x, interactor->dims.y};

            if (wrapsVerts) {
                for (int j = 0; j < numElementsInArray(vals); ++j) {
                    if(vals[j] > 1 && dimArr[j] == Vertex::Phase) {
                        vals[j] -= 1;
                    }
                }
            }

            xBuffer[i] = vals[0];
            yBuffer[i] = vals[1];
        }
    }

    gfx->setCurrentColour(0.8f, 0.0f, 0.55f);
    gfx->drawPoints(vertexSelectedRadius, xy, true);
}

void Panel::handlePendingUpdates() {
    if (Util::assignAndWereDifferent(pendingScaleUpdate, false)) {
        createScales();
        scalesTex->image = scalesImage;
        scalesTex->bind();
    }

    if (Util::assignAndWereDifferent(pendingNameUpdate, false)) {
        nameTexA->image = nameImage;
        nameTexB->image = nameImageB;

        nameTexA->bind();
        nameTexB->bind();
    }

    if (Util::assignAndWereDifferent(pendingDeformUpdate, false)) {
        createDeformerTags();

        dfrmTex->image = dfrmImage;
        dfrmTex->bind();
    }
}

void Panel::drawInterceptsAndHighlightClosest() {
    RasterizerData& data = interactor->getRasterizer()->getRastData();
    const vector<Intercept>& intercepts = data.intercepts;

    int size = 0;
    if(intercepts.empty() && interactor->depthVerts.empty()) {
        return;
    }

    // it's a bigger circle, so do it first
    highlightCurrentIntercept(); {
        ScopedLock dataLock(data.lock);
        size = intercepts.size();

        //  && getSetting(DrawWave) == false
        if (interactor->dims.numHidden() > 0) {
            prepareBuffers(size);

            int numGood = 0;
            for (auto& icpt: intercepts) {
                if (icpt.cube != nullptr) {
                    xBuffer[numGood] = icpt.x;
                    yBuffer[numGood] = icpt.y;

                    ++numGood;
                }
            }

            size = numGood;
        } else {
            prepareAndCopy<Intercept>(intercepts);
        }
    }

    drawScaledInterceptPoints(size);
}

void Panel::updateVertexSizes() {
    vertexWhiteRadius     = 2.f;
    vertexSelectedRadius  = 3.f;
    vertexBlackRadius     = 5.f;
    vertexHighlightRadius = 7.f;

    int scaleSize = getSetting(PointSizeScale);
    while (scaleSize >= ScaleSizes::ScaleMed) {
        vertexWhiteRadius       += 2.f;
        vertexSelectedRadius    += 2.f;
        vertexBlackRadius       += 3.f;
        vertexHighlightRadius   += 4.f;

        scaleSize /= 2;
    }
}

void Panel::drawScaledInterceptPoints(int size) {
    applyScale(xy);

    gfx->setCurrentColour(0, 0, 0, 1);
    gfx->drawPoints(vertexBlackRadius, xy, false);

    gfx->setCurrentColour(1, 1, 1, 1);
    gfx->drawPoints(vertexWhiteRadius, xy, false);
}

void Panel::drawPencilPath() {
    Vertex2 last;
    int size = 0;

    {
        ScopedLock sl(interactor->getLock());
        vector<Vertex2>& path = interactor->pencilPath;

        if(path.empty()) {
            return;
        }

        last = interactor->pencilPath.back();
        size = path.size();
        prepareAndCopy(path);
    }

    gfx->setCurrentColour(0.5f, 0.6f, 1.f);
    gfx->drawLine(last, interactor->state.currentMouse);
    gfx->enableSmoothing();
    gfx->setCurrentColour(1.0f, 0.71f, 0.1f);
    gfx->drawLineStrip(xy, true, true);
}

void Panel::drawOutline() {
    auto right = (float) comp->getWidth();
    auto low = (float) comp->getHeight();

    float y1 = synz(0);
    float y2 = synz(1);
    float x1 = sxnz(0);
    float x2 = sxnz(1);

    gfx->setCurrentColour(0.1f, 0.1f, 0.1f);
    if (vertPadding != 0) {
        gfx->fillRect(0, 0, right, y2, false);
        gfx->fillRect(0, y1, right, low, false);
    }

    if (paddingLeft != 0) {
        gfx->fillRect(0, y1, x1, y2, false);
    }

    if (paddingRight != 0) {
        gfx->fillRect(x2, y1, right, y2, false);
    }

    if (paddingRight + paddingLeft + vertPadding > 0) {
        gfx->disableSmoothing();
        gfx->setCurrentColour(0.2f, 0.2f, 0.2f);
        gfx->drawRect(x1, y1, x2, y2, true);
        gfx->enableSmoothing();
    }
}

void Panel::drawFinalSelection() {
    gfx->drawFinalSelection();
}

void Panel::drawSelectionRectangle() {
    if (interactor == nullptr) {
        return;
    }

    Rectangle<float> selection = interactor->selection.toFloat();

    float lx = selection.getX();
    float rx = selection.getRight();
    float ty = selection.getY();
    float by = selection.getBottom();

    gfx->setCurrentColour(1.0f, 0.7f, 0.15f, 0.23f);
    gfx->fillRect(lx, ty, rx, by, false);

    gfx->disableSmoothing();
    gfx->setCurrentLineWidth(1.f);
    gfx->setCurrentColour(1.0f, 0.7f, 0.15f, 0.45f);
    gfx->drawRect(lx, ty, rx, by, false);
    gfx->enableSmoothing();
}

void Panel::drawBackground(const Rectangle<int>& bounds, bool fillBackground) {
    gfx->drawBackground(bounds, fillBackground);
    gfx->checkErrors();
}

void Panel::applyNoZoomScaleY(Buffer<float> array) {
    float multY = (comp->getHeight() - 2.f * (float) vertPadding);
    float sumY = 1.f;

    array.mul(-1.f).add(sumY).mul(multY).add(vertPadding);
}

void Panel::bakeTexturesNextRepaint() {
    shouldBakeTextures = true;
}

void Panel::setCursor() {
    jassert(comp != nullptr);

    CursorHelper::setCursor(repo, comp, interactor);
}

bool Panel::createLinePath(const Vertex2& first, const Vertex2& second, VertCube* cube, int pointDim, bool haveSpeed) {
    if (cube == nullptr || ! (deformApplicable || speedApplicable)) {
        return false;
    }

    int phsVsTimeChan = cube->deformerAt(Vertex::Time);
    int ampChan       = cube->deformerAt(Vertex::Amp);
    int phsVsRedChan  = cube->deformerAt(Vertex::Red);
    int phsVsBlueChan = cube->deformerAt(Vertex::Blue);

    bool isTime = pointDim == Vertex::Time;
    bool isRed  = pointDim == Vertex::Red;

    int phaseDim            = isTime ? Vertex::Phase : isRed ? Vertex::Red : Vertex::Blue;
    int phaseSrcDim         = isTime ? Vertex::Time : phaseDim;
    int phaseChan           = isTime ? phsVsTimeChan : isRed ? phsVsRedChan : phsVsBlueChan;
    bool adjustSpeed        = haveSpeed && speedApplicable && isTime;
    bool adjustPhase        = phaseSrcDim == pointDim && phaseChan >= 0;
    bool adjustAmp          = ampChan >= 0 && isTime;
    bool anyDfrmAdjustments = (adjustPhase || adjustAmp) && deformApplicable;
    const Dimensions& dims  = interactor->dims;

    if(! anyDfrmAdjustments && (! adjustSpeed || dims.x == Vertex::Phase)) {
        return false;
    }

    prepareBuffers(linestripRes, linestripRes);

    float redOffset  = 0;
    float blueOffset = 0;
    int scratchChan  = getLayerScratchChannel();
    float invSize    = 1 / float(linestripRes - 0.5);
    float phaseGain  = cube->deformerAbsGain(phaseDim);
    float ampGain    = cube->deformerAbsGain(Vertex::Amp);

    bool exHasPhase = adjustPhase && dims.x == Vertex::Phase;
    bool whyHasAmp  = adjustAmp && dims.y == Vertex::Amp;

    std::unique_ptr<ScopedLock> sl;

    if(adjustSpeed) {
        getObj(PathRepo).getLock().enter();
    }

    const PathRepo::ScratchContext& scratchContext = getObj(PathRepo).getScratchContext(scratchChan);

    Buffer<float> redTable, blueTable, phaseTable, ampTable;
    Buffer<float> speedEnv  = scratchContext.panelBuffer;
    Buffer<float> ramp      = cBuffer.withSize(linestripRes);

    if(IDeformer* deformer = interactor->getRasterizer()->getDeformer()) {
        phaseTable = deformer->getTable(phaseChan);
        ampTable = deformer->getTable(ampChan);
    }

    if(speedEnv.empty()) {
        getObj(PathRepo).getLock().exit();
        adjustSpeed = false;
    }

    float xSlope = (second.x - first.x) * invSize;
    float ySlope = (second.y - first.y) * invSize;

    int idx;

    // i.e. is 3d
    if (dims.x != Vertex::Phase) {
        xy.x.ramp(first.x, xSlope);

        if (adjustSpeed) {
            int     speedEnvIdx;
            float   scaleX      = second.x - first.x;
            int     offsetIdx   = first.x * float(speedEnv.size() - 1);
            float   speed;

            if (phsVsTimeChan >= 0) {
                for (int i = 0; i < linestripRes; ++i) {
                    speedEnvIdx = scaleX * i + offsetIdx;       // todo Lerp it
                    speed       = speedEnv[speedEnvIdx];
                    idx         = int((phaseTable.size() - 1) * speed);
                    xy.y[i]     = phaseGain * phaseTable[idx]; // + speed * (second.y - first.y);
                }

                xy.y.addProduct(speedEnv, second.y - first.y).add(first.y + redOffset + blueOffset);
            } else {
                if (scaleX < 0.99f) {
                    for(int i = 0; i < linestripRes; ++i)
                        xy.y[i] = speedEnv[int(scaleX * i) + offsetIdx];        // todo Lerp it
                } else {
                    speedEnv.copyTo(xy.y);
                }

                xy.y.mul(second.y - first.y).add(first.y + redOffset + blueOffset);
            }
        } else {
            if (adjustPhase) {
                xy.y.downsampleFrom(phaseTable);
                xy.y.mul(phaseGain);
            } else {
                xy.y.zero();
            }

            xy.y.add(ramp.ramp(first.y + redOffset + blueOffset, ySlope));
        }
    } else {
        xy.zero();

        bool linTransX = true, linTransY = true;

        if (adjustSpeed) {
            float speed;

            if (exHasPhase || whyHasAmp) {
                for (int i = 0; i < speedEnv.size(); ++i) {
                    speed = speedEnv[i];
                    idx = int((IDeformer::tableSize - 1) * speed);

                    if(exHasPhase) {
                        xy.x[i] = phaseGain * phaseTable[idx] + speed * (second.x - first.x);
                    }

                    if(whyHasAmp) {
                        xy.y[i] = ampGain * ampTable[idx] + speed * (second.y - first.y);
                    }
                }
            }

            linTransX = false; // ! exHasPhase;
            linTransY = false; // ! whyHasAmp;

            if(exHasPhase)
                xy.x.add(first.x + redOffset + blueOffset);
            else {
                VecOps::mul(speedEnv, second.x - first.x, xy.x);
                xy.x.add(first.x + redOffset + blueOffset);
            }

            if(whyHasAmp)
                xy.y.add(first.y);
            else {
                speedEnv.copyTo(xy.y);
                xy.y.mul(second.y - first.y).add(first.y);
            }
        } else {
            if (exHasPhase) {
                xy.x.downsampleFrom(phaseTable);
                xy.x.mul(phaseGain);

                if(redOffset + blueOffset != 0.f)
                    xy.x.add(redOffset + blueOffset);
            }

            if(whyHasAmp) {
                xy.y.downsampleFrom(ampTable);
                xy.y.mul(ampGain);
            }
        }

        if (linTransX) {
            xy.x.add(ramp.ramp(first.x, xSlope));
        }

        if(linTransY) {
            xy.y.add(ramp.ramp(first.y, ySlope));
        }
    }
    getObj(PathRepo).getLock().exit();

    return true;
}

void Panel::componentChanged() {
    comp->setName(panelName);
}

void Panel::createNameImage(const String& displayName, bool isSecondImage, bool brighter) {
    Font font(FontOptions(getStrConstant(FontFace), 20, Font::plain));
    font.setExtraKerningFactor(-0.02);

    String lcName   = displayName.toLowerCase();
    int width       = roundToInt(Util::getStringWidth(font, lcName));
    int pow2        = NumberUtils::nextPower2(width + 3);
    Image tempImg   = Image(Image::ARGB, pow2, 64, true);

    Graphics tempG(tempImg);
    Rectangle r(0, 0, pow2 - 2, 64);

    tempG.setFont(font);
    tempG.setColour(Colours::black);
    tempG.drawText(lcName, r, Justification::topRight, false);

    GlowEffect shadow;
    shadow.setGlowProperties(1.5f, Colours::black);
    shadow.applyEffect(tempImg, tempG, 0.8f, 1.f);

    Image& dest = isSecondImage ? nameImageB : nameImage;
    dest = Image(Image::ARGB, pow2, 64, true);
    Graphics g(dest);

    g.drawImageAt(tempImg, 0, 0);
    g.setFont(font);
    g.setColour(Colour::greyLevel(1.f).withAlpha(brighter ? 0.65f : 0.6f));
    g.drawText(lcName, r, Justification::topRight, false);
}

void Panel::createDeformerTags() {
    int position = 0;
    int fontScale = getSetting(PointSizeScale);
    auto& mg = getObj(MiscGraphics);
    Font& font = *mg.getAppropriateFont(fontScale);

    Image tempImage(Image::ARGB, 512, 16, true);
    Graphics tempG(tempImage);

    tempG.setFont(font);
    tempG.setColour(Colours::black);

    for (int i = 0; i < 32; ++i) {
        String number(i + 1);
        int width = roundToInt(Util::getStringWidth(font, number)) + 1;
        tempG.drawSingleLineText(number, position, (int) font.getHeight());

        position += width;
    }

    dfrmImage = Image(Image::ARGB, 512, 16, true);
    Graphics g(dfrmImage);

    g.drawImageAt(tempImage, 1, 1);
    g.setFont(font);
    g.setColour(Colour::greyLevel(1.f));

    position = 0;
    dfrmTags.clear();

    for(int i = 0; i < 32; ++i) {
        String number(i + 1);
        int width = roundToInt(Util::getStringWidth(font, number)) + 1;
        Rectangle r(position, 0, width, (int) font.getHeight());
        g.drawSingleLineText(number, position, (int) font.getHeight());

        dfrmTags.push_back(r.toFloat());
        position += width;
    }
}

void Panel::triggerZoom(bool in) {
    in ? zoomPanel->zoomIn(false, getWidth() / 2, getHeight() / 2) :
         zoomPanel->zoomOut(false, getWidth() / 2, getHeight() / 2);
}

float Panel::sx(const float x) const {
    return (x - zoomPanel->rect.x) / (zoomPanel->rect.w) *
           (comp->getWidth() - (paddingLeft + paddingRight)) + paddingLeft;
}

float Panel::sxnz(const float x) const {
    return x * (comp->getWidth() - (paddingLeft + paddingRight)) + paddingLeft;
}

float Panel::invertScaleX(const int x) const {
    return (x - paddingLeft) / (float(comp->getWidth() - (paddingLeft + paddingRight))) *
           zoomPanel->rect.w + zoomPanel->rect.x;
}

float Panel::invertScaleXNoZoom(const int x) const { return x / (float(comp->getWidth())); }

float Panel::sy(const float y) const {
    return (1 - zoomPanel->rect.y - y) / (zoomPanel->rect.h) * (comp->getHeight() - 2 * vertPadding) + vertPadding;
}

float Panel::synz(const float y) const { return (1 - y) * (comp->getHeight() - 2 * vertPadding) + vertPadding; }

float Panel::invertScaleY(const int y) const {
    return 1 - zoomPanel->rect.y + zoomPanel->rect.h * (y - vertPadding) / (2 * vertPadding - comp->getHeight());
}

float Panel::invertScaleYNoZoom(const int y) const { return (comp->getHeight() - y) / float(comp->getHeight()); }
float Panel::invScaleXNoDisp(const int x) const { return x / (float(comp->getWidth())) * zoomPanel->rect.w; }
float Panel::invScaleYNoDisp(const int y) const { return (-y) / (float(comp->getHeight())) * zoomPanel->rect.h; }

void Panel::doZoomExtra(bool commandDown) {
    interactor->doZoomExtra(commandDown);
}

void Panel::zoomUpdated(int updateSource) {
    repaint();
}

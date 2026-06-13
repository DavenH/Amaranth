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
#include "../../Curve/Curve.h"
#include "../../Curve/GuideCurveProvider.h"
#include "../../Curve/Mesh/Intercept.h"
#include "../../Curve/Mesh/PathRepo.h"
#include "../../Obj/Color.h"
#include "../../Obj/ColorPoint.h"
#include "../../Obj/CurveLine.h"
#include "../../Inter/Interactor.h"
#include "../../Util/MicroTimer.h"
#include "../../Util/Util.h"

int panelCount = 0;

Panel::Panel(SingletonRepo* repo, const String& name, bool isTransparent) :
        SingletonAccessor       (repo, name)
    ,   isTransparent           (isTransparent)
    ,   alwaysDrawDepthLines    (false)
    ,   backgroundTempoSynced   (false)
    ,   backgroundTimeRelevant  (true)
    ,   guideCurveApplicable    (true)
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
    ,   guideCurveTex           (nullptr)
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

namespace {

PanelRenderer* getRenderer(Panel* panel) {
    return panel->getPanelRenderer();
}

bool shouldDrawPanelPass(const char* passName) {
    const char* ablation = std::getenv("CYCLE_PANEL_ABLATE");

    if (ablation == nullptr) {
        return true;
    }

    String flags(ablation);
    return ! flags.containsIgnoreCase("all")
        && ! flags.containsIgnoreCase(passName);
}

}

Panel::~Panel() {
    gfx = nullptr;
}

void Panel::setGraphicsHelper(CommonGfx* gfx) {
    this->gfx.reset(gfx);
}

void Panel::setComponent(Component* comp) {
    this->comp = comp;
    componentChanged();
    bindInteractorToComponent();
}

void Panel::setInteractor(Interactor* itr) {
    interactor = itr;
    bindInteractorToComponent();
}

void Panel::repaint() {
    requestRepaint();
}

void Panel::requestRepaint(PanelDirtyState::Flag flag) {
    if (hostCallbacks.hasRepaintCallback()) {
        hostCallbacks.requestRepaint(this, flag);
        return;
    }

    if (comp != nullptr) {
        comp->repaint();
    }
}

void Panel::clear() {
    if (renderHelper != nullptr) {
        renderHelper->clear();
    }
}

void Panel::deactivateContext() {
    if (renderHelper != nullptr) {
        renderHelper->deactivate();
    }
}

void Panel::activateContext() {
    if (renderHelper != nullptr) {
        renderHelper->activate();
    }
}

bool Panel::isVisible() const {
    if (hasExplicitHostContext) {
        return hostContext.visible;
    }

    return comp != nullptr && comp->isVisible();
}

int Panel::getWidth() const {
    if (hasExplicitHostContext) {
        return roundToInt(hostContext.bounds.getWidth());
    }

    return comp != nullptr ? comp->getWidth() : 0;
}

int Panel::getHeight() const {
    if (hasExplicitHostContext) {
        return roundToInt(hostContext.bounds.getHeight());
    }

    return comp != nullptr ? comp->getHeight() : 0;
}

Rectangle<int> Panel::getBounds() const {
    if (hasExplicitHostContext) {
        return hostContext.bounds.getSmallestIntegerContainer();
    }

    return comp != nullptr ? comp->getBounds() : Rectangle<int>();
}

Rectangle<int> Panel::getLocalBounds() const {
    if (hasExplicitHostContext) {
        return Rectangle<int>(0, 0, getWidth(), getHeight());
    }

    return comp != nullptr ? comp->getLocalBounds() : Rectangle<int>();
}

bool Panel::isMouseOver() const {
    if (interactor != nullptr) {
        return interactor->state.mouseFlags[PanelState::MouseOver];
    }

    return comp != nullptr && comp->isMouseOver();
}

void Panel::bindInteractorToComponent() {
    if (interactor == nullptr || comp == nullptr) {
        return;
    }

    interactor->associateTo(this);
}

void Panel::render() {
    // should only be held on mesh deletions
    ScopedLock sl(renderLock);

    if (interactor == nullptr || getWidth() <= 0 || getHeight() <= 0 || !interactor->hasRasterizer()) {
        return;
    }

    if (panelRenderer != nullptr) {
        panelRenderer->beginPanelRender(createRenderContext());
    }

    handlePendingUpdates();

    PanelState& state = interactor->state;
    bool sharedCanvasBackground = usesSharedCanvasBackground();
    bool sharedCanvasSurface = usesSharedCanvasSurface();

    if (!sharedCanvasBackground && shouldDrawPanelPass("background")) {
        drawBackground();
    }

    if (shouldBakeTextures && usesCachedSurface() && !sharedCanvasSurface && shouldDrawPanelPass("baked-textures")) {
        bakeTextures();
    }

    bool drawVerts = ! getSetting(ViewVertsOnlyOnHover) || mouseFlag(MouseOver);

    if (shouldDrawPanelPass("pre-draw")) {
        preDraw();
    }

    if(! drawLinesAfterFill && drawVerts && shouldDrawPanelPass("intercept-lines")) {
        drawInterceptLines();
    }

    if (shouldDrawPanelPass("curves")) {
        drawCurvesAndSurfaces();
    }

    if(drawLinesAfterFill && drawVerts && shouldDrawPanelPass("intercept-lines")) {
        drawInterceptLines();
    }

    if (shouldDrawPanelPass("outline")) {
        drawOutline();
    }

    if (shouldDrawPanelPass("post-curve")) {
        postCurveDraw();
    }

    if ((drawVerts || alwaysDrawDepthLines) && shouldDrawPanelPass("depth-lines")) {
        drawDepthLinesAndVerts();
    }

    if (drawVerts) {
        if (shouldDrawPanelPass("intercept-points")) {
            drawInterceptsAndHighlightClosest();
        }

        if (shouldDrawPanelPass("viewable-verts")) {
            drawViewableVerts();
        }

        if (shouldDrawPanelPass("selected-verts")) {
            highlightSelectedVerts();
        }
    }

    if (shouldDrawPanelPass("post-verts")) {
        postVertsDraw();
    }

    if (mouseFlag(MouseOver) && getSetting(DrawScales) && shouldDrawPanelPass("scales")) {
        PanelRenderer* renderer = getRenderer(this);
        jassert(renderer != nullptr);
        renderer->setCurrentColour(Color(1, 1, 1));
        renderer->drawTexture(currentNameId == NameTexture ? nameTexA : nameTexB);

        drawScales();
        drawGuideCurveTags();
    }

    if (mouseFlag(MouseOver) && shouldDrawPanelPass("final-selection")) {
        drawFinalSelection();
    }

    if (shouldDrawPanelPass("pencil")) {
        drawPencilPath();
    }

    if(actionIs(BoxSelecting) && shouldDrawPanelPass("selection-rect")) {
        drawSelectionRectangle();
    }

    if (panelRenderer != nullptr) {
        panelRenderer->endPanelRender();
      #ifdef JUCE_DEBUG
        panelRenderer->checkErrors();
      #endif
    }

    dirtyState.clear(PanelDirtyState::Flag::Layout);
    dirtyState.clear(PanelDirtyState::Flag::StaticVisual);
    dirtyState.clear(PanelDirtyState::Flag::Overlay);
    dirtyState.clear(PanelDirtyState::Flag::Full);
    hasExplicitHostContext = false;
}

void Panel::render(const PanelHostContext& context) {
    setHostContext(context);
    render();
}

void Panel::constrainZoom() {
    ZoomRect& rect = zoomPanel->rect;
    // zoomPanel->validateRect("Panel::constrainZoom-before", false);
    NumberUtils::constrain<float>(rect.w, 0.001f, rect.xMaximum - rect.xMinimum);
    NumberUtils::constrain<float>(rect.h, 0.005f, rect.yMaximum - rect.yMinimum);
    NumberUtils::constrain<float>(rect.x, rect.xMinimum, rect.xMaximum - rect.w);
    NumberUtils::constrain<float>(rect.y, rect.yMinimum, rect.yMaximum - rect.h);

    // zoomPanel->validateRect("Panel::constrainZoom-after");
}

void Panel::drawBackground(bool fillBackground) {
    drawBackground(getLocalBounds(), fillBackground);
}

void Panel::panelResized() {
    dirtyState.mark(PanelDirtyState::Flag::Layout);
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);

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
    int wA = nameImage.getWidth() / textTextureScale;
    int hA = nameImage.getHeight() / textTextureScale;
    int wB = nameImageB.getWidth() > 0 ? nameImageB.getWidth() / textTextureScale : wA;
    int hB = nameImageB.getHeight() > 0 ? nameImageB.getHeight() / textTextureScale : hA;

    Rectangle<int> bounds = getLocalBounds();
    nameTexA->rect = (fromBottom ?  bounds.removeFromRight(wA).removeFromBottom(hA) :
                                    bounds.removeFromRight(wA).removeFromTop(hA)).toFloat();

    Point<float> offset = nameCornerPos;
    nameTexA->rect.translate(offset.getX(), offset.getY());

    bounds = getLocalBounds();
    nameTexB->rect = (fromBottom ?  bounds.removeFromRight(wB).removeFromBottom(hB) :
                                    bounds.removeFromRight(wB).removeFromTop(hB)).toFloat();
    nameTexB->rect.translate(offset.getX(), offset.getY());
}

void Panel::updateBackground(bool onlyVerticalBackground) {
    ScopedLock sl(renderLock);

    int w = getWidth();
    int h = getHeight();

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
        int numLines    = jmax(0, jmin(maxSize, endIdx - startIdx) + 1);
        // DBG(String::formatted("Panel::%s num vert minor lines=%d", name.toUTF8(), numLines));

        vertMinorLines.resize(numLines);
        if (numLines > 0) {
            vertMinorLines.ramp(offset, fineInc.x);
        }
    }

    // coarse
    {
        int startIdx    = jmax(0, (int) ceilf(zoomLeft / crsInc.x));
        int endIdx      = ceilf(zoomRight / crsInc.x);
        float offset    = bgPaddingLeft + startIdx * crsInc.x;
        int maxSize     = jmin(int(maxMajorSize), (int) ceilf(xScale / crsInc.x));
        int numLines    = jmax(0, jmin(maxSize, endIdx - startIdx));

        vertMajorLines.resize(numLines);
        if (numLines > 0) {
            vertMajorLines.ramp(offset, crsInc.x);
        }
    }

    float ratio = float(w * 0.8) / float(jmax(1, h));
    int reductRatio = jmax(1, roundToInt(ratio));

    if (!onlyVerticalBackground) {
        {
            int numLines = int(minorLineOrder / reductRatio);
            numLines     = jlimit(2, (int)maxMinorSize, NumberUtils::nextPower2(numLines));
            fineInc.y    = yScale / numLines;

            horzMinorLines.resize(numLines);
            if (numLines > 0) {
                horzMinorLines.ramp(bgPaddingBttm + fineInc.y, fineInc.y);
            }
        }

        {
            int numLines = int(majorLineOrder / reductRatio);
            numLines     = jlimit(2, (int)maxMajorSize, NumberUtils::nextPower2(numLines));
            crsInc.y     = yScale / numLines;

            horzMajorLines.resize(numLines - 1);
            if (numLines > 1) {
                horzMajorLines.ramp(bgPaddingBttm + crsInc.y, crsInc.y);
            }
        }
    } else {
        // nb: not nullify; want to keep pointer
        horzMinorLines.resize(0);
        horzMajorLines.resize(0);
    }

    pendingScaleUpdate = true;
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
}

void Panel::applyScale(BufferXY& buff) {
    applyScaleX(buff.x);
    applyScaleY(buff.y);
}

void Panel::applyScaleX(Buffer<Float32> array) {
    float sumX = -zoomPanel->rect.x;
    float multX = (getWidth() - paddingLeft - paddingRight) / zoomPanel->rect.w;

    array.add(sumX).mul(multX).add(paddingLeft);
}

void Panel::applyScaleY(Buffer<Float32> array) {
    float multY = (getHeight() - 2 * vertPadding) / zoomPanel->rect.h;
    float sumY = 1 - zoomPanel->rect.y;

    array.mul(-1).add(sumY).mul(multY).add(vertPadding);
}

void Panel::applyNoZoomScaleX(Buffer<Float32> array) {
    float multX = (getWidth() - (paddingLeft + paddingRight));

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

    applyScale(xy);

    PanelRenderer* renderer = getRenderer(this);
    jassert(renderer != nullptr);
    renderer->setCurrentColour(0, 0, 0);
    renderer->drawPoints(vertexBlackRadius, xy, false);
    renderer->setCurrentColour(1, 1, 1);
    renderer->drawPoints(vertexWhiteRadius, xy, false);
}

void Panel::highlightSelectedVerts() {
    int size = 0;
    {
        ScopedLock sl(interactor->getLock());

        vector<Vertex*>& selected = interactor->getSelected();

        if(selected.empty()) {
            return;
        }

        bool wrapsVerts = interactor->rasterizerWrapsVertices();

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

    PanelRenderer* renderer = getRenderer(this);
    jassert(renderer != nullptr);
    renderer->setCurrentColour(0.8f, 0.0f, 0.55f);
    renderer->drawPoints(vertexSelectedRadius, xy, true);
}

void Panel::handlePendingUpdates() {
    if (Util::assignAndWereDifferent(pendingScaleUpdate, false)) {
        createScales();
        scalesTex->setImage(scalesImage, textTextureScale);

        PanelRenderer* renderer = getRenderer(this);
        jassert(renderer != nullptr);
        renderer->updateTexture(scalesTex);
        dirtyState.clear(PanelDirtyState::Flag::StaticVisual);
    }

    if (Util::assignAndWereDifferent(pendingNameUpdate, false)) {
        nameTexA->setImage(nameImage, textTextureScale);
        nameTexB->setImage(nameImageB, textTextureScale);

        PanelRenderer* renderer = getRenderer(this);
        jassert(renderer != nullptr);
        renderer->updateTexture(nameTexA);
        renderer->updateTexture(nameTexB);
        updateNameTexturePos();
        dirtyState.clear(PanelDirtyState::Flag::StaticVisual);
    }

    if (Util::assignAndWereDifferent(pendingDeformUpdate, false)) {
        createGuideCurveTags();

        guideCurveTex->setImage(guideCurveImage, textTextureScale);

        PanelRenderer* renderer = getRenderer(this);
        jassert(renderer != nullptr);
        renderer->updateTexture(guideCurveTex);
        dirtyState.clear(PanelDirtyState::Flag::Resource);
    }
}

void Panel::drawInterceptsAndHighlightClosest() {
    auto snapshot = interactor->rasterizerSnapshot();
    const vector<Intercept>& intercepts = snapshot.intercepts();

    int size = 0;
    if(intercepts.empty() && interactor->depthVerts.empty()) {
        return;
    }

    // it's a bigger circle, so do it first
    highlightCurrentIntercept(); {
        ScopedLock dataLock(snapshot.lock());
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

    PanelRenderer* renderer = getRenderer(this);
    jassert(renderer != nullptr);
    renderer->setCurrentColour(0, 0, 0, 1);
    renderer->drawPoints(vertexBlackRadius, xy, false);
    renderer->setCurrentColour(1, 1, 1, 1);
    renderer->drawPoints(vertexWhiteRadius, xy, false);
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

    PanelRenderer* renderer = getRenderer(this);
    jassert(renderer != nullptr);
    renderer->setCurrentColour(0.5f, 0.6f, 1.f);
    renderer->drawLine(last.x, last.y, interactor->state.currentMouse.x, interactor->state.currentMouse.y, true);
    renderer->enableSmoothing();
    renderer->setCurrentColour(1.0f, 0.71f, 0.1f);
    renderer->drawLineStrip(xy, true);
}

void Panel::drawOutline() {
    auto right = (float) getWidth();
    auto low = (float) getHeight();

    float y1 = synz(0);
    float y2 = synz(1);
    float x1 = sxnz(0);
    float x2 = sxnz(1);

    PanelRenderer* renderer = getRenderer(this);
    jassert(renderer != nullptr);
    renderer->setCurrentColour(0.1f, 0.1f, 0.1f);
    if (vertPadding != 0) {
        renderer->fillRect(0, 0, right, y2, false);
        renderer->fillRect(0, y1, right, low, false);
    }

    if (paddingLeft != 0) {
        renderer->fillRect(0, y1, x1, y2, false);
    }

    if (paddingRight != 0) {
        renderer->fillRect(x2, y1, right, y2, false);
    }

    if (paddingRight + paddingLeft + vertPadding > 0) {
        renderer->disableSmoothing();
        renderer->setCurrentColour(0.2f, 0.2f, 0.2f);
        renderer->drawRect(x1, y1, x2, y2, true);
        renderer->enableSmoothing();
    }
}

void Panel::drawFinalSelection() {
    PanelRenderer* renderer = getRenderer(this);
    jassert(renderer != nullptr);
    renderer->drawFinalSelection();
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

    PanelRenderer* renderer = getRenderer(this);
    jassert(renderer != nullptr);
    renderer->setCurrentColour(1.0f, 0.7f, 0.15f, 0.23f);
    renderer->fillRect(lx, ty, rx, by, false);
    renderer->disableSmoothing();
    renderer->setCurrentLineWidth(1.f);
    renderer->setCurrentColour(1.0f, 0.7f, 0.15f, 0.45f);
    renderer->drawRect(lx, ty, rx, by, false);
    renderer->enableSmoothing();
}

void Panel::drawBackground(const Rectangle<int>& bounds, bool fillBackground) {
    PanelRenderer* renderer = getRenderer(this);
    jassert(renderer != nullptr);
    renderer->drawBackground(bounds, fillBackground);
  #ifdef JUCE_DEBUG
    renderer->checkErrors();
  #endif
}

void Panel::paintSharedCanvasBackground(juce::Graphics& g, const juce::Rectangle<int>& bounds) const {
    g.saveState();
    g.reduceClipRegion(bounds);

    g.setColour(juce::Colour::greyLevel(0.08f));
    g.fillRect(bounds);

    g.setColour(juce::Colour::greyLevel(0.12f));
    g.drawRect(bounds);

    g.restoreState();
}

void Panel::paintSharedCanvasSurface(juce::Graphics& g, const juce::Rectangle<int>& bounds) const {
    ignoreUnused(g, bounds);
}

bool Panel::paintSharedCanvasDebugOverlay(juce::Graphics& g, const juce::Rectangle<int>& bounds) const {
    ignoreUnused(g, bounds);
    return false;
}

void Panel::applyNoZoomScaleY(Buffer<float> array) {
    float multY = (getHeight() - 2.f * (float) vertPadding);
    float sumY = 1.f;

    array.mul(-1.f).add(sumY).mul(multY).add(vertPadding);
}

void Panel::bakeTexturesNextRepaint() {
    shouldBakeTextures = true;
    dirtyState.mark(PanelDirtyState::Flag::SurfaceCache);
}

PanelRenderContext Panel::createRenderContext() const {
    if (hasExplicitHostContext) {
        PanelHostContext context = hostContext;
        context.dirtyMask = dirtyState.mask();
        context.panelId = panelId;
        context.usesCachedSurface = usesCachedSurface();

        return context.createRenderContext();
    }

    return createComponentHostContext().createRenderContext();
}

PanelHostContext Panel::createComponentHostContext(float scaleFactor) const {
    PanelHostContext context;

    if (comp != nullptr) {
        context.bounds = comp->getLocalBounds().toFloat();
        context.clip = context.bounds;
    }

    context.dirtyMask = dirtyState.mask();
    context.panelId = panelId;
    context.scaleFactor = scaleFactor;
    context.usesCachedSurface = usesCachedSurface();
    context.visible = comp != nullptr && comp->isVisible();

    return context;
}

void Panel::setHostContext(const PanelHostContext& context) {
    hostContext = context;
    hasExplicitHostContext = true;

    if (context.callbacks.hasRepaintCallback()) {
        setHostCallbacks(context.callbacks);
    }
}

void Panel::setHostCallbacks(const PanelHostCallbacks& callbacks) {
    hostCallbacks = callbacks;
}

void Panel::setPanelMouseCursor(const MouseCursor& cursor) {
    if (hostCallbacks.hasCursorCallback()) {
        hostCallbacks.setMouseCursor(this, cursor);
        return;
    }

    if (comp != nullptr) {
        comp->setMouseCursor(cursor);
    }
}

void Panel::setCursor() {
    setPanelMouseCursor(CursorHelper::getCursor(repo, interactor));
}

bool Panel::createLinePath(const Vertex2& first, const Vertex2& second, VertCube* cube, int pointDim, bool haveSpeed) {
    if (cube == nullptr || ! (guideCurveApplicable || speedApplicable)) {
        return false;
    }

    int ampChan       = cube->guideCurveAt(Vertex::Amp);

    bool isTime = pointDim == Vertex::Time;

    int phaseDim            = getLinePathPhaseGuideDimension(pointDim);
    int phaseSrcDim         = isTime ? Vertex::Time : phaseDim;
    int phaseChan           = getLinePathPhaseGuideChannel(*cube, pointDim);
    bool adjustSpeed        = haveSpeed && speedApplicable && isTime;
    bool adjustPhase        = phaseSrcDim == pointDim && phaseChan >= 0;
    bool adjustAmp          = ampChan >= 0 && (isTime || pointDim == Vertex::Amp);
    bool anyDfrmAdjustments = (adjustPhase || adjustAmp) && guideCurveApplicable;
    const Dimensions& dims  = interactor->dims;

    if(! anyDfrmAdjustments && (! adjustSpeed || dims.x == Vertex::Phase)) {
        return false;
    }

    prepareBuffers(linestripRes, linestripRes);

    float redOffset  = 0;
    float blueOffset = 0;
    int scratchChan  = getLayerScratchChannel();
    float invSize    = 1 / float(linestripRes - 0.5);
    float phaseGain  = cube->guideCurveAbsGain(phaseDim);
    float ampGain    = cube->guideCurveAbsGain(Vertex::Amp);

    bool exHasPhase = adjustPhase && dims.x == Vertex::Phase;
    bool whyHasAmp  = adjustAmp && dims.y == Vertex::Amp;

    std::unique_ptr<ScopedLock> sl;

    bool lockedPathRepo = adjustSpeed;

    if(lockedPathRepo) {
        getObj(PathRepo).getLock().enter();
    }

    const PathRepo::ScratchContext& scratchContext = getObj(PathRepo).getScratchContext(scratchChan);

    Buffer<float> redTable, blueTable, phaseTable, ampTable;
    Buffer<float> speedEnv  = scratchContext.panelBuffer;
    Buffer<float> ramp      = cBuffer.withSize(linestripRes);

    if(GuideCurveProvider* guideCurveProvider = interactor->getGuideCurveProvider()) {
        phaseTable = guideCurveProvider->getTable(phaseChan);
        ampTable = guideCurveProvider->getTable(ampChan);
    }

    if ((adjustPhase && phaseTable.empty()) || (adjustAmp && ampTable.empty())) {
        if (lockedPathRepo) {
            getObj(PathRepo).getLock().exit();
        }
        return false;
    }

    if(speedEnv.empty()) {
        if (lockedPathRepo) {
            getObj(PathRepo).getLock().exit();
        }
        adjustSpeed = false;
        lockedPathRepo = false;
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
            float   offsetIdx   = first.x * float(speedEnv.size() - 1);
            float   indexScale  = scaleX * float(speedEnv.size() - 1) * invSize;
            float   speed;

            if (adjustPhase) {
                for (int i = 0; i < linestripRes; ++i) {
                    speedEnvIdx = jlimit(0, speedEnv.size() - 1, int(offsetIdx + indexScale * i)); // todo Lerp it
                    speed       = speedEnv[speedEnvIdx];
                    ramp[i]     = speed;
                    idx         = int((phaseTable.size() - 1) * speed);
                    xy.y[i]     = phaseGain * phaseTable[idx];
                }

                xy.y.addProduct(ramp, second.y - first.y).add(first.y + redOffset + blueOffset);
            } else {
                if (scaleX < 0.99f) {
                    for(int i = 0; i < linestripRes; ++i) {
                        speedEnvIdx = jlimit(0, speedEnv.size() - 1, int(offsetIdx + indexScale * i)); // todo Lerp it
                        xy.y[i] = speedEnv[speedEnvIdx];
                    }
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
                    idx = int((GuideCurveProvider::tableSize - 1) * speed);

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
    if (lockedPathRepo) {
        getObj(PathRepo).getLock().exit();
    }

    return true;
}

int Panel::getLinePathPhaseGuideChannel(const VertCube& cube, int pointDim) {
    switch (pointDim) {
        case Vertex::Time: return cube.guideCurveAt(Vertex::Phase);
        case Vertex::Phase: return cube.guideCurveAt(Vertex::Phase);
        case Vertex::Red:  return cube.guideCurveAt(Vertex::Red);
        default:           return cube.guideCurveAt(Vertex::Blue);
    }
}

int Panel::getLinePathPhaseGuideDimension(int pointDim) {
    switch (pointDim) {
        case Vertex::Time: return Vertex::Phase;
        case Vertex::Phase: return Vertex::Phase;
        case Vertex::Red:  return Vertex::Red;
        default:           return Vertex::Blue;
    }
}

void Panel::componentChanged() {
    if (comp != nullptr) {
        comp->setName(panelName);
    }
}

void Panel::createNameImage(const String& displayName, bool isSecondImage, bool brighter) {
    Font font(FontOptions(getStrConstant(FontFace), 16, Font::plain));

    String lcName   = displayName.toLowerCase();
    int width       = roundToInt(Util::getStringWidth(font, lcName));
    int pow2        = NumberUtils::nextPower2(width + 3);
    Image tempImg   = Image(Image::ARGB, pow2 * textTextureScale, 64 * textTextureScale, true);

    Rectangle r(0, 0, pow2 - 2, 64);

    {
        Graphics tempG(tempImg);
        tempG.addTransform(AffineTransform::scale((float) textTextureScale));
        tempG.setFont(font);
        tempG.setColour(Colours::black);
        tempG.drawText(lcName, r, Justification::topRight, false);
    }

    GlowEffect shadow;
    shadow.setGlowProperties(1.5f, Colours::black);
    Graphics tempG(tempImg);
    shadow.applyEffect(tempImg, tempG, 0.8f, 1.f);

    Image& dest = isSecondImage ? nameImageB : nameImage;
    dest = Image(Image::ARGB, pow2 * textTextureScale, 64 * textTextureScale, true);
    Graphics g(dest);

    g.drawImageAt(tempImg, 0, 0);
    g.addTransform(AffineTransform::scale((float) textTextureScale));
    g.setFont(font);
    g.setColour(Colour::greyLevel(1.f).withAlpha(brighter ? 0.65f : 0.6f));
    g.drawText(lcName, r, Justification::topRight, false);

    triggerPendingNameUpdate();
}

void Panel::createGuideCurveTags() {
    int position = 0;
    int fontScale = getSetting(PointSizeScale);
    auto& mg = getObj(MiscGraphics);
    Font& font = *mg.getAppropriateFont(fontScale);

    Image tempImage(Image::ARGB, 512 * textTextureScale, 16 * textTextureScale, true);
    Graphics tempG(tempImage);

    tempG.addTransform(AffineTransform::scale((float) textTextureScale));
    tempG.setFont(font);
    tempG.setColour(Colours::black);

    for (int i = 0; i < 32; ++i) {
        String number(i + 1);
        int width = roundToInt(Util::getStringWidth(font, number)) + 1;
        tempG.drawSingleLineText(number, position, (int) font.getHeight());

        position += width;
    }

    guideCurveImage = Image(Image::ARGB, 512 * textTextureScale, 16 * textTextureScale, true);
    Graphics g(guideCurveImage);

    g.drawImageAt(tempImage, 1, 1);
    g.addTransform(AffineTransform::scale((float) textTextureScale));
    g.setFont(font);
    g.setColour(Colour::greyLevel(1.f));

    position = 0;
    guideCurveTags.clear();

    for(int i = 0; i < 32; ++i) {
        String number(i + 1);
        int width = roundToInt(Util::getStringWidth(font, number)) + 1;
        Rectangle r(position, 0, width, (int) font.getHeight());
        g.drawSingleLineText(number, position, (int) font.getHeight());

        guideCurveTags.push_back(r.toFloat());
        position += width;
    }
}

void Panel::triggerZoom(bool in) {
    in ? zoomPanel->zoomIn(false, getWidth() / 2, getHeight() / 2) :
         zoomPanel->zoomOut(false, getWidth() / 2, getHeight() / 2);
}

float Panel::sx(const float x) const {
    return (x - zoomPanel->rect.x) / (zoomPanel->rect.w) *
           (getWidth() - (paddingLeft + paddingRight)) + paddingLeft;
}

float Panel::sxnz(const float x) const {
    return x * (getWidth() - (paddingLeft + paddingRight)) + paddingLeft;
}

float Panel::invertScaleX(const int x) const {
    return (x - paddingLeft) / (float(getWidth() - (paddingLeft + paddingRight))) *
           zoomPanel->rect.w + zoomPanel->rect.x;
}

float Panel::invertScaleXNoZoom(const int x) const { return x / (float(getWidth())); }

float Panel::sy(const float y) const {
    return (1 - zoomPanel->rect.y - y) / (zoomPanel->rect.h) * (getHeight() - 2 * vertPadding) + vertPadding;
}

float Panel::synz(const float y) const { return (1 - y) * (getHeight() - 2 * vertPadding) + vertPadding; }

float Panel::invertScaleY(const int y) const {
    return 1 - zoomPanel->rect.y + zoomPanel->rect.h * (y - vertPadding) / (2 * vertPadding - getHeight());
}

float Panel::invertScaleYNoZoom(const int y) const { return (getHeight() - y) / float(getHeight()); }
float Panel::invScaleXNoDisp(const int x) const { return x / (float(getWidth())) * zoomPanel->rect.w; }
float Panel::invScaleYNoDisp(const int y) const { return (-y) / (float(getHeight())) * zoomPanel->rect.h; }

void Panel::doZoomExtra(bool commandDown) {
    interactor->doZoomExtra(commandDown);
}

void Panel::zoomUpdated(int updateSource) {
    repaint();
}

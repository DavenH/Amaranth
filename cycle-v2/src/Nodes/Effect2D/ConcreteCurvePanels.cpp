#include "ConcreteCurvePanels.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>
#include <Inter/Interactor2D.h>
#include <Obj/MorphPosition.h>
#include <UI/Panels/CommonGfx.h>
#include <UI/Panels/CurvePanelDrawing.h>
#include <UI/Panels/Panel2D.h>
#include <Util/Arithmetic.h>

#include "../Trimesh/TrimeshPanelEnvironment.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace CycleV2 {

namespace {

constexpr float kGuidePadding = 0.05f;
constexpr float kWaveshaperPadding = 0.125f;
constexpr float kIrPadding = 0.0625f;

}

class FlatCurvePanelBase :
        public Panel2D
    ,   public Interactor2D
    ,   public FlatCurvePanelContract {
public:
    FlatCurvePanelBase(
            SingletonRepo* repo,
            const String& name,
            Mesh& meshToEdit,
            float padding,
            bool bipolar,
            bool verticalPadding) :
            Panel2D         (repo, name, true, true)
        ,   Interactor2D    (repo, name, Dimensions(Vertex::Phase, Vertex::Amp))
        ,   SingletonAccessor(repo, name)
        ,   rasterizer      (repo, name + "Rasterizer")
        ,   mesh            (meshToEdit)
        ,   domainPadding   (padding)
        ,   padVertically   (verticalPadding) {
        vertPadding = 0;
        paddingLeft = 0;
        paddingRight = 0;
        backgroundTimeRelevant = false;
        speedApplicable = false;
        guideCurveApplicable = false;
        alwaysDrawDepthLines = true;
        drawLinesAfterFill = false;
        curveIsBipolar = bipolar;
        bgPaddingLeft = padding;
        bgPaddingRight = padding;
        bgPaddingTop = verticalPadding ? padding : 0.f;
        bgPaddingBttm = verticalPadding ? padding : 0.f;
        colorA = Color(0.92f, 0.93f, 0.96f, 0.92f);
        colorB = colorA;

        rasterizer.setDims(dims);
        rasterizer.setMesh(&mesh);
        Interactor2D::setRasterizer(&rasterizer);
        interactor = this;
        suspendUndo = true;
        vertexProps.sliderApplicable[Vertex::Time] = false;
        vertexProps.sliderApplicable[Vertex::Red] = false;
        vertexProps.sliderApplicable[Vertex::Blue] = false;
        vertexProps.ampVsPhaseApplicable = false;
        for (auto& flag : vertexProps.guideCurveApplicable) {
            flag = false;
        }
        vertexProps.dimensionNames.set(Vertex::Time, {});
        vertexProps.dimensionNames.set(Vertex::Red, {});
        vertexProps.dimensionNames.set(Vertex::Blue, {});
        vertexProps.dimensionNames.set(Vertex::Phase, "x");
        vertexProps.dimensionNames.set(Vertex::Amp, "y");
        vertexLimits[Vertex::Phase] = Range<float>(0.f, 1.f);
    }

    Panel& hostedPanel() override { return *this; }
    void init() override {
        Panel2D::init();
        Interactor2D::init();
    }
    void initWithHost(Component* hostComponent) override {
        Panel2D::initWithExternalComponent(hostComponent);
        Interactor2D::init();
        updateZoomBounds(true);
    }
    void clearInteractionState() override {
        state.currentVertex = nullptr;
        state.currentCube = nullptr;
        state.selectedFrame.clear();
        getSelected().clear();
        resetFinalSelection();
    }
    Vertex* selectedFlatVertexForModel() override {
        if (state.currentVertex != nullptr) {
            return state.currentVertex;
        }
        const auto& selected = getSelected();
        return selected.empty() ? nullptr : selected.front();
    }
    void setControlValues(
            bool enabledToUse,
            float first,
            float second,
            float third,
            int menuId) override {
        enabled = enabledToUse;
        controlA = first;
        controlB = second;
        controlC = third;
        selectedMenuId = menuId;
    }
    bool isMeshEnabled() override { return enabled; }
    Mesh* getMesh() override { return &mesh; }
    bool doCreateVertex() override {
        return addNewCube(0.f, state.currentMouse.x, state.currentMouse.y, 0.f);
    }
    void mouseDoubleClick(const MouseEvent& event) override {
        updateCurrentMouseFromLocalPosition(event.getPosition());
        doCreateVertex();
    }
    bool locateClosestElement() override {
        state.currentIcpt = -1;
        state.currentFreeVert = -1;
        state.currentCube = nullptr;
        return Interactor::locateClosestElement();
    }
    void setExtraElements(float x) override { Interactor2D::setExtraElements(x); }
    float getCurveProximityThreshold() const override { return 20.f; }
    bool addNewCube(float startTime, float x, float y, float curve) override {
        ignoreUnused(startTime);
        auto* vertex = new Vertex(x, y);
        vertex->values[Vertex::Curve] = curve;
        {
            ScopedLock lock(vertexLock);
            mesh.addVertex(vertex);
            state.currentVertex = vertex;
            vector<Vertex*>& selected = getSelected();
            selected.clear();
            selected.push_back(vertex);
            resetFinalSelection();
            updateSelectionFrames();
        }
        refreshRasterizer();
        state.flags[PanelState::DidMeshChange] = true;
        Panel2D::repaint();
        return true;
    }
    void refreshRasterizer() override {
        rasterizer.updateGeometry();
        rasterizer.updateWaveform();
        updateZoomBounds(false);
    }
    void performUpdate(UpdateType updateType) override {
        if (updateType == Update) {
            refreshRasterizer();
        }
        Panel2D::repaint();
    }
    var automationState() const override {
        auto* root = new DynamicObject();
        appendCommonAutomation(*root);
        auto snapshot = const_cast<FXRasterizer&>(rasterizer).snapshotView();
        ScopedLock dataLock(snapshot.lock());
        Array<var> curvePoints;
        for (const Curve& curve : snapshot.curves()) {
            const int resolution = Curve::resolution >> curve.resIndex;
            const int centre = jlimit(0, resolution - 1, resolution / 2);
            auto* point = new DynamicObject();
            point->setProperty("x", curve.transformX[centre]);
            point->setProperty("y", curve.transformY[centre]);
            point->setProperty("controlX", curve.b.x);
            curvePoints.add(point);
        }
        root->setProperty("curvePoints", curvePoints);
        Array<var> waveformPoints;
        const Buffer<Float32> waveX = snapshot.waveX();
        const Buffer<Float32> waveY = snapshot.waveY();
        for (int index = 0; index < jmin(waveX.size(), waveY.size()); ++index) {
            auto* point = new DynamicObject();
            point->setProperty("x", waveX[index]);
            point->setProperty("y", waveY[index]);
            waveformPoints.add(point);
        }
        root->setProperty("waveformPoints", waveformPoints);
        root->setProperty("curveHover", mouseFlag(WithinReshapeThresh));
        return var(root);
    }
    std::vector<TrimeshVertexParameter> selectedVertexParameters() const override {
        const auto& selected = const_cast<FlatCurvePanelBase*>(this)->getSelected();
        const Vertex* vertex = !selected.empty() ? selected.front() : firstEditableVertex();
        if (vertex == nullptr) {
            return {};
        }
        return {
            { "vertex.phase", "phase", vertex->values[Vertex::Phase], 0.f, 1.f },
            { "vertex.amp", "amp", vertex->values[Vertex::Amp], 0.f, 1.f },
            { "vertex.curve", "curve", vertex->values[Vertex::Curve], 0.f, 1.f }
        };
    }
    bool setSelectedVertexParameter(
            const String& parameterId,
            float normalizedValue) override {
        const auto& selected = getSelected();
        Vertex* vertex = !selected.empty() ? selected.front() : firstEditableVertex();
        if (vertex == nullptr) {
            return false;
        }
        const int dimension = vertexDimensionForParameter(parameterId);
        if (dimension < 0) {
            return false;
        }
        vertex->values[dimension] = jlimit(0.f, 1.f, normalizedValue);
        state.currentVertex = vertex;
        refreshRasterizer();
        state.flags[PanelState::DidMeshChange] = true;
        listeners.call(&InteractorListener::selectionChanged, getMesh(), state.selectedFrame);
        Panel2D::repaint();
        return true;
    }
    void updateZoomBounds(bool resetView) override {
        if (zoomPanel == nullptr) {
            return;
        }
        const float xMinimum = 0.5f * domainPadding;
        const float xMaximum = 1.f - 0.5f * domainPadding;
        const float yMinimum = padVertically ? 0.5f * domainPadding : 0.f;
        const float yMaximum = padVertically ? 1.f - 0.5f * domainPadding : 1.f;
        zoomPanel->rect.xMinimum = xMinimum;
        zoomPanel->rect.xMaximum = xMaximum;
        zoomPanel->rect.yMinimum = yMinimum;
        zoomPanel->rect.yMaximum = yMaximum;
        if (resetView) {
            zoomPanel->rect.x = xMinimum;
            zoomPanel->rect.w = xMaximum - xMinimum;
            zoomPanel->rect.y = yMinimum;
            zoomPanel->rect.h = yMaximum - yMinimum;
        }
        panel->constrainZoom();
    }

protected:
    void doReshapeCurve(const MouseEvent& event) override {
        ignoreUnused(event);
        auto snapshot = rasterizerSnapshot();
        if (snapshot.curves().empty()) {
            return;
        }

        flag(LoweredRes) = true;
        const vector<Curve>& curves = snapshot.curves();
        {
            ScopedLock lock(vertexLock);
            vector<Vertex*>& selected = getSelected();
            selected.clear();
            if (state.currentVertex != nullptr) {
                selected.push_back(state.currentVertex);
                updateSelectionFrames();
            }
        }
        resetFinalSelection();

        const Array<Vertex*> movingVertices = getVerticesToMove(state.currentCube, state.currentVertex);
        const float dragScale = getDragMovementScale(state.currentCube);
        if (state.currentVertex == nullptr) {
            return;
        }
        const float diffY = (state.currentMouse.y - state.lastMouse.y)
                / sqrtf(panel->getZoomPanel()->rect.h);
        const Curve& curve = curves[(size_t) getStateValue(CurrentCurve)];
        const float gesturePole = state.start.y < state.currentVertex->values[dims.y]
                ? 1.f
                : -1.f;
        const float delta = diffY * dragScale * gesturePole
                / (0.1f + curve.tp.scaleY);

        for (auto* vertex : movingVertices) {
            float& weight = vertex->values[Vertex::Curve];
            weight += delta;
            NumberUtils::constrain(weight, 0.f, 1.f);
        }

        listeners.call(&InteractorListener::selectionChanged, getMesh(), state.selectedFrame);
        flag(DidMeshChange) |= delta != 0.f;
    }

    CurvePanelDrawing::Canvas drawingCanvas() {
        return {
            *gfx,
            getWidth(),
            getHeight(),
            [this](float x) { return sx(x); },
            [this](float y) { return sy(y); }
        };
    }
    float firstControl() const { return controlA; }
    float secondControl() const { return controlB; }
    float thirdControl() const { return controlC; }

private:
    void appendCommonAutomation(DynamicObject& root) const {
        if (zoomPanel != nullptr) {
            auto* zoom = new DynamicObject();
            zoom->setProperty("x", zoomPanel->rect.x);
            zoom->setProperty("y", zoomPanel->rect.y);
            zoom->setProperty("w", zoomPanel->rect.w);
            zoom->setProperty("h", zoomPanel->rect.h);
            zoom->setProperty("xMinimum", zoomPanel->rect.xMinimum);
            zoom->setProperty("xMaximum", zoomPanel->rect.xMaximum);
            zoom->setProperty("yMinimum", zoomPanel->rect.yMinimum);
            zoom->setProperty("yMaximum", zoomPanel->rect.yMaximum);
            root.setProperty("zoom", var(zoom));
        }
        if (state.currentVertex != nullptr) {
            auto* vertex = new DynamicObject();
            vertex->setProperty("x", state.currentVertex->values[Vertex::Phase]);
            vertex->setProperty("y", state.currentVertex->values[Vertex::Amp]);
            vertex->setProperty("curve", state.currentVertex->values[Vertex::Curve]);
            root.setProperty("currentVertex", var(vertex));
        }
        root.setProperty("movingVertexCount", (int) state.selectedFrame.size());
        root.setProperty("hasCurrentCube", false);
        Array<var> parameters;
        for (const auto& parameter : selectedVertexParameters()) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("id", parameter.id);
            encoded->setProperty("value", parameter.value);
            parameters.add(encoded);
        }
        root.setProperty("selectedVertexParameters", parameters);
    }
    Vertex* firstEditableVertex() const {
        const auto& vertices = mesh.getVerts();
        return vertices.empty() ? nullptr : vertices.front();
    }
    static int vertexDimensionForParameter(const String& parameterId) {
        const String field = parameterId.fromLastOccurrenceOf(".", false, false);
        if (field == "phase") { return Vertex::Phase; }
        if (field == "amp") { return Vertex::Amp; }
        if (field == "curve") { return Vertex::Curve; }
        return -1;
    }

    FXRasterizer rasterizer;
    Mesh& mesh;
    float domainPadding {};
    bool padVertically {};
    bool enabled { true };
    float controlA { 0.5f };
    float controlB { 0.5f };
    float controlC { 0.5f };
    int selectedMenuId {};
};

class EnvelopeCurvePanel final :
        public Panel2D
    ,   public Interactor2D
    ,   public EnvelopeCurvePanelContract {
friend class Effect2DPanelBridge;

public:
    Panel& hostedPanel() override { return *this; }

    EnvelopeCurvePanel(
            SingletonRepo* repo,
            TrimeshPanelEnvironment& panelEnvironment,
            EnvelopeMesh& meshToEdit) :
            Panel2D         (repo, "CycleV2EnvelopePanel", true, true)
        ,   Interactor2D    (repo, "CycleV2EnvelopePanel",
                    Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Red, Vertex::Blue))
        ,   SingletonAccessor(repo, "CycleV2EnvelopePanel")
        ,   envRasterizer   (nullptr, "CycleV2EnvelopePanelRasterizer")
        ,   environment     (panelEnvironment)
        ,   mesh            (meshToEdit)
        ,   envelopeMesh    (meshToEdit) {
        vertPadding = 0;
        paddingLeft = 0;
        paddingRight = 0;
        backgroundTimeRelevant = false;
        curveIsBipolar = false;
        speedApplicable = false;
        guideCurveApplicable = false;
        alwaysDrawDepthLines = true;
        drawLinesAfterFill = false;

        colorA = Color(0.92f, 0.93f, 0.96f, 0.92f);
        colorB = Color(0.92f, 0.93f, 0.96f, 0.92f);

        envRasterizer.setDims(dims);
        envRasterizer.setMesh(&envelopeMesh);
        Interactor2D::setRasterizer(&envRasterizer);
        interactor = this;
        suspendUndo = true;

        vertexProps.sliderApplicable[Vertex::Time] = false;
        vertexProps.sliderApplicable[Vertex::Red] = false;
        vertexProps.sliderApplicable[Vertex::Blue] = false;
        vertexProps.ampVsPhaseApplicable = false;

        for (auto& flag : vertexProps.guideCurveApplicable) {
            flag = false;
        }

        vertexProps.dimensionNames.set(Vertex::Time, {});
        vertexProps.dimensionNames.set(Vertex::Red, {});
        vertexProps.dimensionNames.set(Vertex::Blue, {});
        vertexProps.dimensionNames.set(Vertex::Phase, "x");
        vertexProps.dimensionNames.set(Vertex::Amp, "y");
    }

    void init() override {
        Panel2D::init();
        Interactor2D::init();
    }

    void initWithHost(Component* hostComponent) override {
        Panel2D::initWithExternalComponent(hostComponent);
        Interactor2D::init();
        updateZoomBounds(true);
        updateEnvelopeBackgroundGrid();
    }

    void clearInteractionState() override {
        state.currentVertex = nullptr;
        state.currentCube = nullptr;
        state.selectedFrame.clear();
        getSelected().clear();
        resetFinalSelection();
    }

    VertCube* selectedEnvelopeCubeForModel() override {
        const auto& selected = getSelected();
        if (selected.empty()) {
            return nullptr;
        }
        if (state.currentCube != nullptr && selected.front()->isOwnedBy(state.currentCube)) {
            return state.currentCube;
        }
        return closestEnvelopeCubeFor(selected.front());
    }

    bool isEffectEnabled() const { return enabled; }
    bool isMeshEnabled() override { return isEffectEnabled(); }
    Mesh* getMesh() override { return &mesh; }

    void setEnvelopeLogarithmic(bool shouldUseLogarithmicScale) override {
        envelopeLogarithmic = shouldUseLogarithmicScale;
        updateEnvelopeBackgroundGrid();
        Panel2D::repaint();
    }

    void setEnvelopeAxisLinks(bool redLinked, bool blueLinked) override {
        const ScopedLock lock(vertexLock);
        if (envelopeRedLinked == redLinked && envelopeBlueLinked == blueLinked) {
            return;
        }

        envelopeRedLinked = redLinked;
        envelopeBlueLinked = blueLinked;

        if (positioner != nullptr) {
            updateSelectionFrames();
            Panel2D::repaint();
        }
    }

    void setControlValues(
            bool isEnabled,
            float firstValue,
            float secondValue,
            float thirdValue,
            int menuId) override {
        enabled = isEnabled;
        controlA = firstValue;
        controlB = secondValue;
        controlC = thirdValue;
        selectedMenuId = menuId;

        applyEnvelopeMorphPosition(false);
    }

    bool doCreateVertex() override {
        return addNewCube(0.f, state.currentMouse.x, state.currentMouse.y, 0.f);
    }

    void mouseDoubleClick(const MouseEvent& event) override {
        updateCurrentMouseFromLocalPosition(event.getPosition());
        doCreateVertex();
    }

    bool locateClosestElement() override {
        return Interactor2D::locateClosestElement();
    }
    float getCurveProximityThreshold() const override { return 20.f; }

    void setExtraElements(float x) override {
        ScopedLock lock(vertexLock);

        if (state.currentCube != nullptr) {
            state.currentVertex = findLinesClosestVertex(
                    state.currentCube,
                    state.currentMouse,
                    envelopeMorphPosition());
            jassert(state.currentVertex == nullptr
                    || state.currentVertex->owners.contains(state.currentCube));
        }
    }

    bool addNewCube(float startTime, float x, float y, float curve) override {
        ignoreUnused(startTime);
        return addEnvelopeCubeAt(x, y, curve);
    }

    void transferLineProperties(VertCube const* from, VertCube* to1, VertCube* to2) override {
        Interactor::transferLineProperties(from, to1, to2);

        if (from == nullptr) {
            return;
        }

        auto transferMarker = [from, to1, to2](std::set<VertCube*>& markers) {
            auto* source = const_cast<VertCube*>(from);

            if (markers.find(source) == markers.end()) {
                return;
            }

            markers.erase(source);
            markers.insert(to1);
            markers.insert(to2);
        };

        transferMarker(envelopeMesh.loopCubes);
        transferMarker(envelopeMesh.sustainCubes);
    }

    Array<Vertex*> getVerticesToMove(VertCube* cube, Vertex* startVertex) override {
        if (cube == nullptr || startVertex == nullptr) {
            return Interactor::getVerticesToMove(cube, startVertex);
        }

        Array<Vertex*> moving;
        moving.add(startVertex);
        if (Vertex* timePair = cube->getOtherVertexAlong(Vertex::Time, startVertex)) {
            moving.addIfNotAlreadyThere(timePair);
        }

        if (envelopeRedLinked && envelopeBlueLinked) {
            moving = cube->toArray();
        } else if (envelopeRedLinked) {
            moving = cube->getFace(Vertex::Blue, startVertex).toArray();
        } else if (envelopeBlueLinked) {
            moving = cube->getFace(Vertex::Red, startVertex).toArray();
        }

        return moving;
    }

    void doReshapeCurve(const MouseEvent& event) override {
        auto snapshot = rasterizerSnapshot();

        if (snapshot.curves().empty()) {
            return;
        }

        flag(LoweredRes) = true;
        const vector<Curve>& curves = snapshot.curves();

        {
            ScopedLock sl(vertexLock);
            vector<Vertex*>& selected = getSelected();
            selected.clear();

            if (state.currentVertex != nullptr) {
                selected.push_back(state.currentVertex);
                updateSelectionFrames();
            }
        }

        resetFinalSelection();

        Array<Vertex*> movingVerts = getVerticesToMove(state.currentCube, state.currentVertex);
        if (state.currentVertex == nullptr) {
            return;
        }
        const Curve& curve = curves[(size_t) getStateValue(CurrentCurve)];
        const float diffY = (state.currentMouse.y - state.lastMouse.y)
                / sqrtf(panel->getZoomPanel()->rect.h);
        const float gesturePole = state.start.y < state.currentVertex->values[dims.y]
                ? 1.f
                : -1.f;
        const float diff = diffY * gesturePole / (0.1f + curve.tp.scaleY);

        for (auto* vertex : movingVerts) {
            float& weight = vertex->values[Vertex::Curve];
            weight += diff;
            NumberUtils::constrain(weight, 0.f, 1.f);
        }

        listeners.call(&InteractorListener::selectionChanged, getMesh(), state.selectedFrame);
        flag(DidMeshChange) |= diff != 0.f;
    }

    void doExtraMouseDrag(const MouseEvent& event) override {
        Interactor2D::doExtraMouseDrag(event);

        if ((actionIs(PanelState::DraggingVertex)
                        || actionIs(PanelState::ReshapingCurve)
                        || actionIs(PanelState::DraggingCorner))
                && synchronizeEnvelopeLoopSeamForCurrentEdit()) {
            envRasterizer.updateWaveform(&mesh, 0.f);
            state.flags[PanelState::DidMeshChange] = true;
            Panel2D::repaint();
        }
    }

    void setMovingVertsFromSelected() override {
        state.selectedFrame.clear();
        vector<Vertex*>& selected = getSelected();

        for (auto* vertex : selected) {
            VertCube* cube = state.currentCube != nullptr && vertex == state.currentVertex
                    ? state.currentCube
                    : closestEnvelopeCubeFor(vertex);
            addToArray(getVerticesToMove(cube, vertex), state.selectedFrame);
        }
    }

    void refreshRasterizer() override {
        envRasterizer.setMorphPosition(envelopeMorphPosition());
        envRasterizer.updateWaveform(&mesh, 0.f);
        validateEnvelopeMarkers();
        if (synchronizeEnvelopeLoopSeamForCurrentEdit()) {
            envRasterizer.updateWaveform(&mesh, 0.f);
            validateEnvelopeMarkers();
        }

        updateZoomBounds(false);
    }

    void performUpdate(UpdateType updateType) override {
        if (updateType == Update) {
            refreshRasterizer();
        }

        Panel2D::repaint();
    }

    void preDraw() override {
        drawEnvelopeBackground();
    }

    void postCurveDraw() override {
        drawEnvelopeBounds();
    }

    void drawCurvesAndSurfaces() override {
        if (!drawEnvelopeCurveAndSections()) {
            Panel2D::drawCurvesAndSurfaces();
        }
    }

    var automationState() const override {
        auto* root = new DynamicObject();

        if (zoomPanel != nullptr) {
            auto* zoom = new DynamicObject();
            zoom->setProperty("x", zoomPanel->rect.x);
            zoom->setProperty("y", zoomPanel->rect.y);
            zoom->setProperty("w", zoomPanel->rect.w);
            zoom->setProperty("h", zoomPanel->rect.h);
            zoom->setProperty("xMinimum", zoomPanel->rect.xMinimum);
            zoom->setProperty("xMaximum", zoomPanel->rect.xMaximum);
            zoom->setProperty("yMinimum", zoomPanel->rect.yMinimum);
            zoom->setProperty("yMaximum", zoomPanel->rect.yMaximum);
            root->setProperty("zoom", var(zoom));
        }

        if (state.currentVertex != nullptr) {
            auto* vertex = new DynamicObject();
            vertex->setProperty("x", state.currentVertex->values[Vertex::Phase]);
            vertex->setProperty("y", state.currentVertex->values[Vertex::Amp]);
            vertex->setProperty("time", state.currentVertex->values[Vertex::Time]);
            vertex->setProperty("red", state.currentVertex->values[Vertex::Red]);
            vertex->setProperty("blue", state.currentVertex->values[Vertex::Blue]);
            vertex->setProperty("curve", state.currentVertex->values[Vertex::Curve]);
            root->setProperty("currentVertex", var(vertex));
        }

        root->setProperty("movingVertexCount", (int) state.selectedFrame.size());
        root->setProperty("hasCurrentCube", state.currentCube != nullptr);
        root->setProperty("curveHover", mouseFlag(WithinReshapeThresh));
        Array<var> vertexParameters;
        for (const auto& parameter : selectedVertexParameters()) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("id", parameter.id);
            encoded->setProperty("value", parameter.value);
            vertexParameters.add(encoded);
        }
        root->setProperty("selectedVertexParameters", vertexParameters);

        {
            auto snapshot = const_cast<EnvRasterizer&>(envRasterizer).snapshotView();
            ScopedLock dataLock(snapshot.lock());
            const auto& intercepts = snapshot.intercepts();
            auto* morph = new DynamicObject();
            morph->setProperty("red", controlA);
            morph->setProperty("blue", controlB);
            root->setProperty("morph", var(morph));
            root->setProperty("colorPointCount", (int) snapshot.colorPoints().size());
            root->setProperty("redLinked", envelopeRedLinked);
            root->setProperty("blueLinked", envelopeBlueLinked);

            if (!intercepts.empty()) {
                Array<var> interceptValues;
                auto interceptToVar = [](const Intercept& intercept) {
                    auto* object = new DynamicObject();
                    object->setProperty("x", intercept.x);
                    object->setProperty("y", intercept.y);
                    object->setProperty("curve", intercept.shp);
                    return var(object);
                };

                for (const auto& intercept : intercepts) {
                    interceptValues.add(interceptToVar(intercept));
                }

                root->setProperty("intercepts", interceptValues);
                root->setProperty("firstIntercept", interceptToVar(intercepts.front()));
                root->setProperty("midIntercept", interceptToVar(intercepts[intercepts.size() / 2]));
                root->setProperty("lastIntercept", interceptToVar(intercepts.back()));
            }
        }

        return var(root);
    }

    std::vector<TrimeshVertexParameter> selectedVertexParameters() const override {
        const auto& selected = const_cast<EnvelopeCurvePanel*>(this)->getSelected();
        const Vertex* vertex = !selected.empty() ? selected.front() : firstEditableVertex();

        if (vertex == nullptr) {
            return {};
        }

        return {
                { "vertex.red", "red", vertex->values[Vertex::Red], 0.f, 1.f },
                { "vertex.blue", "blue", vertex->values[Vertex::Blue], 0.f, 1.f },
                { "vertex.phase", "phase", vertex->values[Vertex::Phase], 0.f, 1.5f },
                { "vertex.amp", "amp", vertex->values[Vertex::Amp], 0.f, 1.f },
                { "vertex.curve", "curve", vertex->values[Vertex::Curve], 0.f, 1.f }
        };
    }

    bool setSelectedVertexParameter(
            const String& parameterId,
            float normalizedValue) override {
        const auto& selected = getSelected();
        Vertex* vertex = !selected.empty() ? selected.front() : firstEditableVertex();

        if (vertex == nullptr) {
            return false;
        }

        const float value = parameterValueFromNormalized(parameterId, normalizedValue);
        const int dimension = vertexDimensionForParameter(parameterId);

        if (dimension < 0) {
            return false;
        }

        Array<Vertex*> vertices;

        if (dimension == Vertex::Phase || dimension == Vertex::Amp || dimension == Vertex::Curve) {
            VertCube* cube = selectedEnvelopeCube();
            if (cube == nullptr) {
                cube = closestEnvelopeCubeFor(vertex);
            }

            vertices = getVerticesToMove(cube, vertex);
            if (vertices.isEmpty() && cube != nullptr) {
                for (int i = 0; i < (int) VertCube::numVerts; ++i) {
                    vertices.addIfNotAlreadyThere(cube->getVertex(i));
                }
            }
        } else {
            vertices.add(vertex);
        }

        for (Vertex* movingVertex : vertices) {
            if (movingVertex != nullptr) {
                movingVertex->values[dimension] = value;
            }
        }

        state.currentVertex = vertex;
        refreshRasterizer();
        state.flags[PanelState::DidMeshChange] = true;
        listeners.call(&InteractorListener::selectionChanged, getMesh(), state.selectedFrame);
        Panel2D::repaint();
        return true;
    }

    VertCube* selectedEnvelopeCube() const {
        if (state.currentVertex == nullptr) {
            return state.currentCube;
        }

        auto snapshot = const_cast<EnvRasterizer&>(envRasterizer).snapshotView();
        ScopedLock dataLock(snapshot.lock());
        VertCube* cube = nullptr;

        for (const auto& intercept : snapshot.intercepts()) {
            if (intercept.cube == nullptr) {
                continue;
            }

            for (int i = 0; i < state.currentVertex->getNumOwners(); ++i) {
                if (intercept.cube == state.currentVertex->owners[i]) {
                    cube = intercept.cube;
                }
            }
        }

        return cube != nullptr ? cube : state.currentCube;
    }

    bool selectedEnvelopeMarkerState(bool loopMarker) const override {
        VertCube* cube = selectedEnvelopeCube();

        if (cube == nullptr) {
            return false;
        }

        const auto& markers = loopMarker ? envelopeMesh.loopCubes : envelopeMesh.sustainCubes;
        return markers.find(cube) != markers.end();
    }

    void toggleSelectedEnvelopeMarker(bool loopMarker) override {
        VertCube* cube = selectedEnvelopeCube();

        if (cube == nullptr) {
            return;
        }

        auto& markers = loopMarker ? envelopeMesh.loopCubes : envelopeMesh.sustainCubes;
        const bool wasSet = markers.find(cube) != markers.end();

        markers.clear();

        if (!wasSet) {
            markers.insert(cube);
        }

        envRasterizer.evaluateLoopSustainIndices();
        synchronizeEnvelopeLoopSeamFrom(cube->getVertex(0), loopMarker);
        refreshRasterizer();
        state.flags[PanelState::DidMeshChange] = true;
        Panel2D::repaint();
    }

protected:
    MorphPosition envelopeMorphPosition() const {
        MorphPosition position;
        position.time.setValueDirect(0.f);
        position.red.setValueDirect(controlA);
        position.blue.setValueDirect(controlB);
        position.timeDepth = 0.001f;
        position.redDepth = 1.f;
        position.blueDepth = 1.f;
        return position;
    }

    void applyEnvelopeMorphPosition(bool repaintPanel) {
        const MorphPosition position = envelopeMorphPosition();
        environment.setMorphPosition(position, Vertex::Red);
        envRasterizer.setMorphPosition(position);
        envRasterizer.updateWaveform(&mesh, 0.f);

        if (repaintPanel) {
            Panel2D::repaint();
        }
    }

    bool addEnvelopeCubeAt(float x, float y, float curve) {
        ScopedLock lock(vertexLock);

        MorphPosition position;
        position.time.setValueDirect(0.f);
        position.timeDepth = 0.001f;
        position.red.setValueDirect(0.f);
        position.redDepth = 1.f;
        position.blue.setValueDirect(0.f);
        position.blueDepth = 1.f;

        auto* cube = new VertCube(&envelopeMesh);
        cube->initVerts(position);

        for (int i = 0; i < (int) VertCube::numVerts; ++i) {
            Vertex* vertex = cube->getVertex(i);
            vertex->values[Vertex::Phase] = x;
            vertex->values[Vertex::Amp] = y;

            if (curve >= 0.f) {
                vertex->values[Vertex::Curve] = curve;
            }
        }

        envelopeMesh.addCube(cube);
        state.currentCube = cube;
        state.currentVertex = cube->getVertex(0);

        vector<Vertex*>& selected = getSelected();
        selected.clear();
        selected.push_back(state.currentVertex);
        resetFinalSelection();
        updateSelectionFrames();

        refreshRasterizer();
        state.flags[PanelState::DidMeshChange] = true;
        Panel2D::repaint();
        return true;
    }

    Vertex* firstEditableVertex() const {
        const auto& vertices = mesh.getVerts();
        return vertices.empty() ? nullptr : vertices.front();
    }

    static int vertexDimensionForParameter(const String& parameterId) {
        const String field = parameterId.fromLastOccurrenceOf(".", false, false);

        if (field == "time") { return Vertex::Time; }
        if (field == "red") { return Vertex::Red; }
        if (field == "blue") { return Vertex::Blue; }
        if (field == "phase") { return Vertex::Phase; }
        if (field == "amp") { return Vertex::Amp; }
        if (field == "curve") { return Vertex::Curve; }

        return -1;
    }

    float parameterValueFromNormalized(const String& parameterId, float normalizedValue) const {
        const float normalized = jlimit(0.f, 1.f, normalizedValue);

        if (parameterId.endsWithIgnoreCase(".phase")) {
            return normalized * 1.5f;
        }

        return normalized;
    }

    VertCube* closestEnvelopeCubeFor(Vertex* vertex) {
        if (vertex == nullptr || vertex->owners.size() == 0) {
            return nullptr;
        }

        MorphPosition position;
        position.time.setValueDirect(0.f);
        position.red.setValueDirect(controlA);
        position.blue.setValueDirect(controlB);

        float minDistance = std::numeric_limits<float>::max();
        VertCube* closest = nullptr;

        for (auto* owner : vertex->owners) {
            if (owner == nullptr) {
                continue;
            }

            owner->getFinalIntercept(reduceData, position);

            if (reduceData.pointOverlaps) {
                return owner;
            }

            const float distance = position.distanceTo(owner->getCentroid(true));

            if (distance < minDistance) {
                minDistance = distance;
                closest = owner;
            }
        }

        return closest;
    }

    void updateZoomBounds(bool resetView) override {
        if (zoomPanel == nullptr) {
            return;
        }

        float maxX = 1.5f;
        MorphPosition position;

        for (VertCube* cube : mesh.getCubes()) {
            if (cube != nullptr) {
                maxX = jmax(maxX, envelopeMesh.getPositionOfCubeAt(cube, position));
            }
        }

        zoomPanel->rect.xMinimum = 0.f;
        zoomPanel->rect.xMaximum = maxX;
        zoomPanel->rect.yMinimum = 0.f;
        zoomPanel->rect.yMaximum = 1.f;

        if (resetView) {
            zoomPanel->rect.x = 0.f;
            zoomPanel->rect.w = maxX;
            zoomPanel->rect.y = 0.f;
            zoomPanel->rect.h = 1.f;
        }

        panel->constrainZoom();
        updateEnvelopeBackgroundGrid();
    }

    void updateEnvelopeBackgroundGrid() {
        if (zoomPanel == nullptr) {
            return;
        }

        if (envelopeLogarithmic) {
            Panel::updateBackground(false);
            horzMinorLines.resize(16);

            float level = 1.f;
            for (int i = 0; i < 16; ++i) {
                level *= 0.5f;
                horzMinorLines[i] = Arithmetic::logMapping(30.f, level);
            }
            horzMajorLines.resize(0);
            triggerPendingScaleUpdate();
        } else {
            Panel::updateBackground(false);
        }
    }

    void validateEnvelopeMarkers() {
        std::set<VertCube*> meshCubes;
        for (VertCube* cube : mesh.getCubes()) {
            if (cube != nullptr) {
                meshCubes.insert(cube);
            }
        }

        auto removeMissingCubes = [&meshCubes](std::set<VertCube*>& markers) {
            for (auto iter = markers.begin(); iter != markers.end();) {
                if (meshCubes.find(*iter) == meshCubes.end()) {
                    iter = markers.erase(iter);
                } else {
                    ++iter;
                }
            }
        };

        removeMissingCubes(envelopeMesh.loopCubes);
        removeMissingCubes(envelopeMesh.sustainCubes);

        envRasterizer.evaluateLoopSustainIndices();

        int loopIdx = -1;
        int sustIdx = -1;
        envRasterizer.getIndices(loopIdx, sustIdx);

        if (sustIdx >= 0) {
            auto snapshot = envRasterizer.snapshotView();
            ScopedLock dataLock(snapshot.lock());
            const auto& intercepts = snapshot.intercepts();

            for (int i = 0; i < (int) intercepts.size(); ++i) {
                VertCube* cube = intercepts[(size_t) i].cube;
                if (cube != nullptr
                        && envelopeMesh.loopCubes.find(cube) != envelopeMesh.loopCubes.end()
                        && i > sustIdx - EnvRasterizer::loopMinSizeIcpts) {
                    envelopeMesh.loopCubes.erase(cube);
                }
            }
        }

        envRasterizer.evaluateLoopSustainIndices();
    }

    void drawEnvelopeBackground() {
        const int right = getWidth();
        const int bottom = 0;
        const int top = getHeight();
        const int releaseStart = sx(envelopeReleaseStart());

        gfx->setCurrentColour(0.5f, 0.71f, 1.0f, 0.13f);
        gfx->fillRect(0, top, releaseStart, bottom, false);
        gfx->setCurrentColour(0.6f, 0.45f, 0.25f, 0.16f);
        gfx->fillRect(releaseStart, top, right, bottom, false);
    }

    void drawEnvelopeBounds() {
        const int left = 1;
        const int right = getWidth() - 1;
        const int bottom = 1;
        const int top = getHeight() - 1;

        gfx->disableSmoothing();
        gfx->setCurrentLineWidth(1.f);
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.38f);
        gfx->drawLine(left, top, right, top, false);
        gfx->drawLine(left, bottom, right, bottom, false);
        gfx->drawLine(left, top, left, bottom, false);
        gfx->drawLine(right, top, right, bottom, false);
        gfx->enableSmoothing();
    }

    CurvePanelDrawing::Canvas drawingCanvas() {
        return {
            *gfx,
            getWidth(),
            getHeight(),
            [this](float x) { return sx(x); },
            [this](float y) { return sy(y); }
        };
    }

    float envelopeReleaseStart() const {
        int loopIndex = -1;
        int sustainIndex = -1;
        envRasterizer.getIndices(loopIndex, sustainIndex);
        auto snapshot = const_cast<EnvRasterizer&>(envRasterizer).snapshotView();
        ScopedLock dataLock(snapshot.lock());
        const auto& intercepts = snapshot.intercepts();

        if (sustainIndex >= 0 && sustainIndex < (int) intercepts.size() && intercepts[(size_t) sustainIndex].cube != nullptr) {
            return intercepts[(size_t) sustainIndex].x;
        }

        return 0.76f;
    }

    bool drawEnvelopeCurveAndSections() {
        auto snapshot = envRasterizer.snapshotView();
        ScopedLock dataLock(snapshot.lock());

        const vector<Intercept>& icpts = snapshot.intercepts();
        Buffer<float> waveX = snapshot.waveX();
        Buffer<float> waveY = snapshot.waveY();
        const float stopPosition = sx(icpts.empty() ? 1.f : icpts.back().x);

        if (waveX.empty() || waveY.empty() || icpts.empty()) { return false; }

        const int istart = jmax(0, snapshot.zeroIndex() - 4);
        const int size = waveX.size() - istart;

        if (size <= 1) { return false; }

        prepareBuffers(size, size);
        xy.copyFrom(waveX.offset(istart), waveY.offset(istart));

        Buffer<float> alpha = cBuffer.withSize(size);
        VecOps::mul(xy.y, 0.25f, alpha);
        alpha.abs().subCRev(0.5f).mul(0.75f);
        applyScale(xy);

        Color sectionColours[] = {
                Color(0.5f, 0.71f, 1.0f, 0.75f),
                Color(0.6f, 0.45f, 0.25f, 0.75f),
                Color(0.5f, 0.2f, 0.25f, 0.75f)
        };

        const float loopStart = envelopeLoopStart();
        const float sustain = envelopeReleaseStart();
        const float changePoints[] = { sx(loopStart), sx(sustain) };
        const float baseY = sy(0.f);
        const float baseAlpha = 0.15f;

        vector<ColorPos> positions;
        positions.reserve((size_t) size + 8);

        int colourIndex = loopStart < 0.f ? 1 : 0;
        ColorPos curr;

        if (loopStart < 0.f) {
            sectionColours[1] = sectionColours[0];
        }

        gfx->setCurrentLineWidth(interactor->mouseFlag(WithinReshapeThresh) ? 2.f : 1.f);

        int i = 0;

        while (i < size - 1 && xy.x[i + 1] < 0.f) {
            ++i;
        }

        int changePointIndex = 0;

        while (changePointIndex < numElementsInArray(changePoints)
                && xy.x[i] > changePoints[changePointIndex]) {
            ++changePointIndex;
        }

        colourIndex = jlimit(0, (int) numElementsInArray(sectionColours) - 1, changePointIndex);
        curr.update(xy.x[i] - 5.f, xy.y[i], sectionColours[(size_t) colourIndex].withAlpha(alpha.front()));
        positions.push_back(curr);

        for (; i < size; ++i) {
            const bool isLast = i == size - 1;
            curr.update(xy.x[i], xy.y[i], sectionColours[(size_t) colourIndex].withAlpha(alpha[i]));
            positions.push_back(curr);

            for (int j = 0; !isLast && j < numElementsInArray(changePoints); ++j) {
                const float changePoint = changePoints[j];
                const float low = jmin(xy.x[i], xy.x[i + 1]);
                const float high = jmax(xy.x[i], xy.x[i + 1]);

                if (changePoint >= low && changePoint <= high) {
                    const float xDelta = xy.x[i + 1] - xy.x[i];
                    const float factor = xDelta == 0.f ? 0.f : (changePoint - xy.x[i]) / xDelta;
                    const float y = xy.y[i] + (xy.y[i + 1] - xy.y[i]) * factor;

                    if (j < 1) {
                        curr.update(changePoint, y, sectionColours[(size_t) colourIndex].withAlpha(alpha[i]));
                        positions.push_back(curr);
                    }

                    colourIndex = j + 1;
                    curr.update(changePoint + 0.0001f,
                            y,
                            sectionColours[(size_t) colourIndex].withAlpha(alpha[i]));
                    positions.push_back(curr);
                }
            }

            const float stopLow = jmin(xy.x[i], xy.x[jmin(i + 1, size - 1)]);
            const float stopHigh = jmax(xy.x[i], xy.x[jmin(i + 1, size - 1)]);

            if (!isLast && stopPosition >= stopLow && stopPosition <= stopHigh) {
                const float xDelta = xy.x[i + 1] - xy.x[i];
                const float factor = xDelta == 0.f ? 0.f : (stopPosition - xy.x[i]) / xDelta;
                const float y = xy.y[i] + (xy.y[i + 1] - xy.y[i]) * factor;

                curr.update(stopPosition, y, sectionColours[(size_t) colourIndex].withAlpha(alpha[i]));
                positions.push_back(curr);
                break;
            }
        }

        if (!positions.empty()) {
            gfx->fillAndOutlineColoured(positions, baseY, baseAlpha, true, true);
        }

        gfx->setCurrentLineWidth(1.f);
        return true;
    }

    float envelopeLoopStart() const {
        int loopIndex = -1;
        int sustainIndex = -1;
        envRasterizer.getIndices(loopIndex, sustainIndex);
        auto snapshot = const_cast<EnvRasterizer&>(envRasterizer).snapshotView();
        ScopedLock dataLock(snapshot.lock());
        const auto& intercepts = snapshot.intercepts();

        if (loopIndex >= 0 && loopIndex < (int) intercepts.size() && intercepts[(size_t) loopIndex].cube != nullptr) {
            return intercepts[(size_t) loopIndex].x;
        }

        return -1.f;
    }

    bool synchronizeEnvelopeLoopSeamFrom(Vertex* sourceVertex, bool sourceIsLoopVertex) {
        ignoreUnused(sourceVertex);

        int loopIdx = -1;
        int sustIdx = -1;
        envRasterizer.getIndices(loopIdx, sustIdx);

        auto snapshot = envRasterizer.snapshotView();
        ScopedLock dataLock(snapshot.lock());
        const auto& intercepts = snapshot.intercepts();

        VertCube* loopCube = loopIdx >= 0 && loopIdx < (int) intercepts.size()
                ? intercepts[(size_t) loopIdx].cube
                : nullptr;
        VertCube* sustainCube = sustIdx >= 0 && sustIdx < (int) intercepts.size()
                ? intercepts[(size_t) sustIdx].cube
                : nullptr;

        if (loopCube != nullptr && sustainCube != nullptr) {
            VertCube* toMove = sourceIsLoopVertex ? sustainCube : loopCube;
            VertCube* toCopyFrom = sourceIsLoopVertex ? loopCube : sustainCube;
            bool changed = false;

            for (int i = 0; i < (int) VertCube::numVerts; ++i) {
                Vertex* target = toMove->getVertex(i);
                Vertex* source = toCopyFrom->getVertex(i);
                const float nextAmp = source->values[Vertex::Amp];
                changed = changed || target->values[Vertex::Amp] != nextAmp;
                target->values[Vertex::Amp] = nextAmp;

                if (toMove->getCompGuideCurve() < 0) {
                    target->setMaxSharpness();
                }

                if (toCopyFrom->getCompGuideCurve() < 0) {
                    source->setMaxSharpness();
                }
            }

            return changed;
        }

        if (sustainCube != nullptr && sustainCube->guideCurveAt(Vertex::Time) < 0) {
            for (int i = 0; i < (int) VertCube::numVerts; ++i) {
                sustainCube->getVertex(i)->setMaxSharpness();
            }

            return true;
        }

        return false;
    }

    bool synchronizeEnvelopeLoopSeamForCurrentEdit() {
        const auto isMarked = [](const std::set<VertCube*>& markers, VertCube* cube) {
            return cube != nullptr && markers.find(cube) != markers.end();
        };

        bool movingLoopLine = false;
        bool movingSustainLine = false;
        Vertex* sourceVertex = state.currentVertex;

        vector<Vertex*>& selected = getSelected();
        for (Vertex* vertex : selected) {
            if (vertex == nullptr) {
                continue;
            }

            for (int i = 0; i < vertex->getNumOwners(); ++i) {
                VertCube* owner = vertex->owners[i];
                if (isMarked(envelopeMesh.loopCubes, owner)) {
                    movingLoopLine = true;
                    sourceVertex = vertex;
                }
                if (isMarked(envelopeMesh.sustainCubes, owner)) {
                    movingSustainLine = true;
                    sourceVertex = vertex;
                }
            }
        }

        if (selected.empty()) {
            movingLoopLine = isMarked(envelopeMesh.loopCubes, state.currentCube);
            movingSustainLine = isMarked(envelopeMesh.sustainCubes, state.currentCube);
        }

        if (movingLoopLine && movingSustainLine) {
            return false;
        }

        if (!movingLoopLine && !movingSustainLine) {
            return false;
        }

        return synchronizeEnvelopeLoopSeamFrom(sourceVertex, movingLoopLine);
    }

    EnvRasterizer envRasterizer;
    TrimeshPanelEnvironment& environment;
    Mesh& mesh;
    EnvelopeMesh& envelopeMesh;
    bool enabled { true };
    float controlA { 0.5f };
    float controlB { 0.5f };
    float controlC { 0.5f };
    int selectedMenuId {};
    bool envelopeLogarithmic {};
    bool envelopeRedLinked { true };
    bool envelopeBlueLinked { true };
};

class WaveshaperCurvePanel final : public FlatCurvePanelBase {
public:
    WaveshaperCurvePanel(SingletonRepo* repo, Mesh& mesh) :
            FlatCurvePanelBase(
                    repo, "CycleV2WaveshaperPanel", mesh, kWaveshaperPadding, false, true)
        ,   SingletonAccessor(repo, "CycleV2WaveshaperPanel") {}

    void postCurveDraw() override {
        auto canvas = drawingCanvas();
        CurvePanelDrawing::drawWaveshaperBounds(canvas, kWaveshaperPadding);
    }
};

class GuideCurvePanel final : public FlatCurvePanelBase {
public:
    GuideCurvePanel(SingletonRepo* repo, Mesh& mesh) :
            FlatCurvePanelBase(repo, "CycleV2GuideCurvePanel", mesh, kGuidePadding, true, false)
        ,   SingletonAccessor(repo, "CycleV2GuideCurvePanel") {}

    void preDraw() override {
        auto canvas = drawingCanvas();
        CurvePanelDrawing::drawGuideBackground(canvas, {
            kGuidePadding, firstControl(), secondControl(), thirdControl()
        });
    }
};

class ImpulseResponseCurvePanel final : public FlatCurvePanelBase {
public:
    ImpulseResponseCurvePanel(SingletonRepo* repo, Mesh& mesh) :
            FlatCurvePanelBase(
                    repo, "CycleV2ImpulseResponsePanel", mesh, kIrPadding, true, false)
        ,   SingletonAccessor(repo, "CycleV2ImpulseResponsePanel") {}

    void preDraw() override {
        auto canvas = drawingCanvas();
        CurvePanelDrawing::drawImpulseResponseBackground(canvas, kIrPadding);
    }
    void postCurveDraw() override {
        auto canvas = drawingCanvas();
        CurvePanelDrawing::drawImpulseResponseBounds(canvas, kIrPadding);
    }
};

std::unique_ptr<FlatCurvePanelContract> createFlatCurvePanel(
        NodeKind kind,
        SingletonRepo* repo,
        Mesh& mesh) {
    if (kind == NodeKind::GuideCurve) {
        return std::make_unique<GuideCurvePanel>(repo, mesh);
    }
    if (kind == NodeKind::ImpulseResponse) {
        return std::make_unique<ImpulseResponseCurvePanel>(repo, mesh);
    }
    jassert(kind == NodeKind::Waveshaper);
    return std::make_unique<WaveshaperCurvePanel>(repo, mesh);
}

std::unique_ptr<EnvelopeCurvePanelContract> createEnvelopeCurvePanel(
        SingletonRepo* repo,
        TrimeshPanelEnvironment& environment,
        EnvelopeMesh& mesh) {
    return std::make_unique<EnvelopeCurvePanel>(repo, environment, mesh);
}

}

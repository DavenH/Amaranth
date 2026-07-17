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

    Panel& hostedPanel() override {
        return *this;
    }

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
    void restoreFlatSelection(Vertex* vertex) override {
        clearInteractionState();
        if (vertex == nullptr) {
            return;
        }
        state.currentVertex = vertex;
        getSelected().push_back(vertex);
        updateSelectionFrames();
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
    bool isMeshEnabled() override {
        return enabled;
    }

    Mesh* getMesh() override {
        return &mesh;
    }

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
    void setExtraElements(float x) override {
        Interactor2D::setExtraElements(x);
    }

    float getCurveProximityThreshold() const override {
        return 20.f;
    }
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
        auto snapshot = rasterizer.snapshotView();
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
            [this](float x) {
                return sx(x);
            },
            [this](float y) {
                return sy(y);
            }
        };
    }
    float firstControl() const {
        return controlA;
    }

    float secondControl() const {
        return controlB;
    }

    float thirdControl() const {
        return controlC;
    }

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
        if (field == "phase") {
            return Vertex::Phase;
        }

        if (field == "amp") {
            return Vertex::Amp;
        }

        if (field == "curve") {
            return Vertex::Curve;
        }

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

}

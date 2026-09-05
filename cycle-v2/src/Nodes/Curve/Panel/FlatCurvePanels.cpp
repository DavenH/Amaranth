#include "Nodes/Curve/Panel/ConcreteCurvePanels.h"

#include <Binary/Gradients.h>
#include <Audio/CycleDsp/IrModel.h>
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
#include <UI/ColorGradient.h>
#include <Util/Arithmetic.h>

#include "Nodes/ImpulseResponse/ImpulseResponseAnalysis.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelEnvironment.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace CycleV2 {

namespace {

constexpr float kGuidePadding = 0.05f;
constexpr float kWaveshaperPadding = 0.125f;

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
            float leftPadding,
            float rightPadding,
            float verticalPadding,
            bool bipolar) :
            Panel2D         (repo, name, true, true)
        ,   Interactor2D    (repo, name, Dimensions(Vertex::Phase, Vertex::Amp))
        ,   SingletonAccessor(repo, name)
        ,   rasterizer      (repo, name + "Rasterizer")
        ,   mesh            (meshToEdit)
        ,   domainPaddingLeft    (leftPadding)
        ,   domainPaddingRight   (rightPadding)
        ,   domainPaddingVertical(verticalPadding) {
        vertPadding = 0;
        paddingLeft = 0;
        paddingRight = 0;
        backgroundTimeRelevant = false;
        speedApplicable = false;
        guideCurveApplicable = false;
        alwaysDrawDepthLines = true;
        drawLinesAfterFill = false;
        curveIsBipolar = bipolar;
        bgPaddingLeft = leftPadding;
        bgPaddingRight = rightPadding;
        bgPaddingTop = verticalPadding;
        bgPaddingBttm = verticalPadding;
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
        initialiseInteraction();
    }
    void initWithHost(Component* hostComponent) override {
        Panel2D::initWithExternalComponent(hostComponent);
        initialiseInteraction();
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
        if (interactionInitialised) {
            updateSelectionFrames();
        }
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
    std::vector<CurvePanelGridLine> verticalMajorGridLines() const override {
        std::vector<CurvePanelGridLine> result;
        if (zoomPanel == nullptr || zoomPanel->rect.w <= 0.f) {
            return result;
        }
        result.reserve((size_t) vertMajorLines.size());
        for (int index = 0; index < vertMajorLines.size(); ++index) {
            const float domainX = vertMajorLines[index];
            result.push_back({ domainX, sx(domainX) });
        }
        return result;
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
        const float xMinimum = 0.5f * domainPaddingLeft;
        const float xMaximum = 1.f - 0.5f * domainPaddingRight;
        const float yMinimum = 0.5f * domainPaddingVertical;
        const float yMaximum = 1.f - 0.5f * domainPaddingVertical;
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
    void initialiseInteraction() {
        Interactor2D::init();
        interactionInitialised = true;
        if (state.currentVertex != nullptr) {
            updateSelectionFrames();
        }
    }

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
        Array<var> majorGridLines;
        for (const auto& line : verticalMajorGridLines()) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("domainX", line.domainX);
            encoded->setProperty("panelX", line.panelX);
            majorGridLines.add(var(encoded));
        }
        root.setProperty("verticalMajorGridLines", majorGridLines);
        if (state.currentVertex != nullptr) {
            auto* vertex = new DynamicObject();
            vertex->setProperty("x", state.currentVertex->values[Vertex::Phase]);
            vertex->setProperty("y", state.currentVertex->values[Vertex::Amp]);
            vertex->setProperty("curve", state.currentVertex->values[Vertex::Curve]);
            root.setProperty("currentVertex", var(vertex));
        }
        root.setProperty("movingVertexCount", (int) state.selectedFrame.size());
        root.setProperty("hasCurrentCube", false);
        root.setProperty("firstControl", controlA);
        root.setProperty("secondControl", controlB);
        root.setProperty("thirdControl", controlC);
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
    float domainPaddingLeft {};
    float domainPaddingRight {};
    float domainPaddingVertical {};
    bool interactionInitialised {};
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
                    repo, "CycleV2WaveshaperPanel", mesh,
                    kWaveshaperPadding, kWaveshaperPadding, kWaveshaperPadding, false)
        ,   SingletonAccessor(repo, "CycleV2WaveshaperPanel") {}

    void postCurveDraw() override {
        auto canvas = drawingCanvas();
        CurvePanelDrawing::drawWaveshaperBounds(canvas, kWaveshaperPadding);
    }
};

class GuideCurvePanel final : public FlatCurvePanelBase {
public:
    GuideCurvePanel(SingletonRepo* repo, Mesh& mesh) :
            FlatCurvePanelBase(
                    repo, "CycleV2GuideCurvePanel", mesh,
                    kGuidePadding, kGuidePadding, 0.f, true)
        ,   SingletonAccessor(repo, "CycleV2GuideCurvePanel") {}

    void preDraw() override {
        auto canvas = drawingCanvas();
        CurvePanelDrawing::drawGuideBackground(canvas, {
            kGuidePadding, firstControl(), secondControl(), thirdControl()
        });
    }
};

class ImpulseResponseCurvePanel final : public ImpulseResponseCurvePanelContract,
                                        public FlatCurvePanelBase {
public:
    ImpulseResponseCurvePanel(SingletonRepo* repo, Mesh& mesh) :
            FlatCurvePanelBase(
                    repo, "CycleV2ImpulseResponsePanel", mesh,
                    CycleDsp::irDomainPadding, 0.f, 0.f, true)
        ,   SingletonAccessor(repo, "CycleV2ImpulseResponsePanel") {
        Image image = PNGImageFormat::loadFrom(
                Gradients::burntalum_png, Gradients::burntalum_pngSize);
        gradient.read(image, false, true);
        gradient.multiplyAlpha(0.4f);
    }

    void setImpulseResponseAnalysis(
            std::shared_ptr<const ImpulseResponseAnalysis> nextAnalysis) override {
        analysis = std::move(nextAnalysis);
        spectrumColours.clear();
        if (analysis == nullptr) {
            return;
        }

        const auto& gradientColours = gradient.getColours();
        spectrumColours.reserve(analysis->normalizedMagnitudes.size());
        for (float magnitude : analysis->normalizedMagnitudes) {
            const int index = jlimit(
                    0,
                    (int) gradientColours.size() - 1,
                    (int) (magnitude * (float) (gradientColours.size() - 1)));
            spectrumColours.push_back(gradientColours[(size_t) index]);
        }
        xBuffer.ensureSize((int) analysis->filteredDisplayImpulse.size());
        yBuffer.ensureSize(jmax(
                (int) analysis->filteredDisplayImpulse.size(),
                (int) analysis->frequencyRows.size()));
        spliceBuffer.ensureSize((int) analysis->filteredDisplayImpulse.size() * 2);
    }

    void zoomToAttack() override {
        if (zoomPanel == nullptr) {
            return;
        }
        zoomPanel->rect.x = CycleDsp::irDomainPadding;
        zoomPanel->rect.w *= 0.2f;
        zoomPanel->panelZoomChanged(false);
    }

    void resetZoom() override {
        updateZoomBounds(true);
        if (zoomPanel != nullptr) {
            zoomPanel->panelZoomChanged(false);
        }
    }

    void preDraw() override {
        auto canvas = drawingCanvas();
        CurvePanelDrawing::drawImpulseResponseBackground(
                canvas, CycleDsp::irDomainPadding);
        if (analysis == nullptr) {
            return;
        }

        const int spectrumSize = (int) analysis->frequencyRows.size();
        if (spectrumSize > 0 && spectrumColours.size() == (size_t) spectrumSize) {
            Buffer<float> yScale = yBuffer.withSize(spectrumSize);
            VecOps::copy(analysis->frequencyRows.data(), yScale.get(), spectrumSize);
            applyNoZoomScaleY(yScale);
            gfx->drawVerticalGradient(
                    sx(CycleDsp::irDomainPadding),
                    sx(1.f),
                    yScale,
                    spectrumColours);
        }

        const int impulseSize = (int) analysis->filteredDisplayImpulse.size();
        if (impulseSize > 0) {
            prepareBuffers(impulseSize);
            VecOps::copy(
                    analysis->filteredDisplayImpulse.data(), xy.y.get(), impulseSize);
            xy.y.mul(CycleDsp::irPostGain(secondControl()));
            Arithmetic::unpolarize(xy.y);
            xy.x.ramp(
                    CycleDsp::irDomainPadding,
                    impulseSize > 1
                            ? (1.f - CycleDsp::irDomainPadding) / (float) (impulseSize - 1)
                            : 0.f);
            gfx->enableSmoothing();
            gfx->setCurrentLineWidth(1.5f);
            gfx->setCurrentColour(1.f, 0.62f, 0.7f, 0.75f);
            gfx->drawLineStrip(xy, true, true);
            gfx->setCurrentLineWidth(1.f);
        }
    }
    void postCurveDraw() override {
        auto canvas = drawingCanvas();
        CurvePanelDrawing::drawImpulseResponseBounds(
                canvas, CycleDsp::irDomainPadding);
    }

    void updateBackground(bool onlyVerticalBackground = false) override {
        FlatCurvePanelBase::updateBackground(onlyVerticalBackground);
        const ScopedLock scopedLock(renderLock);
        if (zoomPanel != nullptr
                && 1.f >= zoomPanel->rect.x
                && 1.f <= zoomPanel->rect.x + zoomPanel->rect.w + 0.00001f
                && (vertMajorLines.empty()
                        || !approximatelyEqual(vertMajorLines.back(), 1.f))) {
            const int previousSize = vertMajorLines.size();
            vertMajorLines.resize(previousSize + 1);
            vertMajorLines[previousSize] = 1.f;
        }
    }

    var automationState() const override {
        var state = FlatCurvePanelBase::automationState();
        if (auto* object = state.getDynamicObject()) {
            object->setProperty(
                    "irSpectrumPointCount",
                    analysis != nullptr ? (int) analysis->normalizedMagnitudes.size() : 0);
            object->setProperty(
                    "irFilteredImpulsePointCount",
                    analysis != nullptr
                            ? (int) analysis->filteredDisplayImpulse.size()
                            : 0);
            object->setProperty(
                    "irFilteredImpulseFirstSample",
                    analysis != nullptr && !analysis->filteredDisplayImpulse.empty()
                            ? analysis->filteredDisplayImpulse.front()
                            : 0.f);
            object->setProperty(
                    "irAudioImpulseFirstSample",
                    analysis != nullptr && !analysis->filteredImpulse.empty()
                            ? analysis->filteredImpulse.front()
                            : 0.f);
            object->setProperty(
                    "irDisplayedImpulseFirstSample",
                    analysis != nullptr && !analysis->filteredDisplayImpulse.empty()
                            ? analysis->filteredDisplayImpulse.front()
                                    * CycleDsp::irPostGain(secondControl())
                            : 0.f);
            object->setProperty(
                    "irDisplayGain",
                    CycleDsp::irPostGain(secondControl()));
            object->setProperty("irBackdropRenderer", "OpenGL");
        }
        return state;
    }

private:
    ColorGradient gradient;
    std::shared_ptr<const ImpulseResponseAnalysis> analysis;
    std::vector<Color> spectrumColours;
};

std::unique_ptr<FlatCurvePanelContract> createFlatCurvePanel(
        NodeKind kind,
        SingletonRepo* repo,
        Mesh& mesh) {
    if (kind == NodeKind::ImpulseResponse) {
        return std::make_unique<ImpulseResponseCurvePanel>(repo, mesh);
    }
    jassert(kind == NodeKind::Waveshaper);
    return std::make_unique<WaveshaperCurvePanel>(repo, mesh);
}

std::unique_ptr<FlatCurvePanelContract> createGuideCurvePanel(
        SingletonRepo* repo,
        Mesh& mesh) {
    return std::make_unique<GuideCurvePanel>(repo, mesh);
}

}

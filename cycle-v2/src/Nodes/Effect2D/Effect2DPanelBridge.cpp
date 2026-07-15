#include "Effect2DPanelBridge.h"

#include "../Envelope/EnvelopeMeshState.h"

#include <Curve/Mesh/VertCube.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>
#include <Inter/Interactor2D.h>
#include <Obj/MorphPosition.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>
#include <UI/Panels/PanelHostContext.h>
#include <UI/Panels/Panel2D.h>
#include <Util/Arithmetic.h>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace gl;

namespace CycleV2 {

namespace {

constexpr float kGuidePadding = 0.05f;
constexpr float kWaveshaperPadding = 0.125f;
constexpr float kIrPadding = 0.0625f;

bool isEffect2DNode(NodeKind kind) {
    return kind == NodeKind::Envelope
        || kind == NodeKind::GuideCurve
        || kind == NodeKind::ImpulseResponse
        || kind == NodeKind::Waveshaper;
}

}

class Effect2DPanelBridge::EffectPanel :
        public Panel2D
    ,   public Interactor2D {
friend class Effect2DPanelBridge;

public:
    EffectPanel(
            SingletonRepo* repo,
            const String& name,
            TrimeshPanelEnvironment& panelEnvironment,
            Mesh& meshToEdit,
            EnvelopeMesh* envelopeMeshToEdit) :
            Panel2D         (repo, name, true, true)
        ,   Interactor2D    (repo, name, Dimensions(Vertex::Phase, Vertex::Amp))
        ,   SingletonAccessor(repo, name)
        ,   fxRasterizer    (repo, name + "FXRasterizer")
        ,   envRasterizer   (nullptr, name + "EnvRasterizer")
        ,   environment     (panelEnvironment)
        ,   mesh            (meshToEdit)
        ,   envelopeMesh    (envelopeMeshToEdit) {
        vertPadding = 0;
        paddingLeft = 0;
        paddingRight = 0;
        backgroundTimeRelevant = false;
        speedApplicable = false;
        guideCurveApplicable = false;
        alwaysDrawDepthLines = true;
        drawLinesAfterFill = true;

        colorA = Color(0.92f, 0.93f, 0.96f, 0.92f);
        colorB = Color(0.92f, 0.93f, 0.96f, 0.92f);

        fxRasterizer.setDims(dims);
        fxRasterizer.setMesh(&mesh);
        envRasterizer.setDims(dims);
        if (envelopeMesh != nullptr) {
            envRasterizer.setMesh(envelopeMesh);
        }
        Interactor2D::setRasterizer(&fxRasterizer);
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

    void setNodeKind(NodeKind nodeKind) {
        kind = nodeKind;
        dims = kind == NodeKind::Envelope
                ? Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Red, Vertex::Blue)
                : Dimensions(Vertex::Phase, Vertex::Amp);
        fxRasterizer.setDims(dims);
        envRasterizer.setDims(dims);
        ignoresTime = kind == NodeKind::Envelope;
        curveIsBipolar = kind != NodeKind::Waveshaper && kind != NodeKind::Envelope;
        bgPaddingLeft = paddingForKind();
        bgPaddingRight = (kind == NodeKind::ImpulseResponse || kind == NodeKind::Envelope) ? 0.f : paddingForKind();
        bgPaddingTop = kind == NodeKind::Waveshaper ? paddingForKind() : 0.f;
        bgPaddingBttm = kind == NodeKind::Waveshaper ? paddingForKind() : 0.f;
        Rasterization::Rasterizer* rasterizer = kind == NodeKind::Envelope
                ? static_cast<Rasterization::Rasterizer*>(&envRasterizer)
                : static_cast<Rasterization::Rasterizer*>(&fxRasterizer);
        Interactor2D::setRasterizer(rasterizer);
        vertexProps.isEnvelope = kind == NodeKind::Envelope;
        vertexProps.sliderApplicable[Vertex::Time] = kind != NodeKind::Envelope;
        vertexLimits[Vertex::Phase] = kind == NodeKind::Envelope
                ? Range<float>(0.f, 2.f)
                : Range<float>(0.f, 1.f);
    }

    void init() override {
        Panel2D::init();
        Interactor2D::init();
    }

    void initWithHost(Component* hostComponent) {
        Panel2D::initWithExternalComponent(hostComponent);
        Interactor2D::init();
        updateZoomBounds(true);
        updateEnvelopeBackgroundGrid();
    }

    void clearInteractionState() {
        state.currentVertex = nullptr;
        state.currentCube = nullptr;
        state.selectedFrame.clear();
        getSelected().clear();
        resetFinalSelection();
    }

    Vertex* selectedFlatVertexForModel() {
        if (state.currentVertex != nullptr) {
            return state.currentVertex;
        }
        const auto& selected = getSelected();
        return selected.empty() ? nullptr : selected.front();
    }

    VertCube* selectedEnvelopeCubeForModel() {
        if (kind != NodeKind::Envelope) {
            return nullptr;
        }
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

    void setEnvelopeLogarithmic(bool shouldUseLogarithmicScale) {
        envelopeLogarithmic = shouldUseLogarithmicScale;
        updateEnvelopeBackgroundGrid();
        Panel2D::repaint();
    }

    void setEnvelopeAxisLinks(bool redLinked, bool blueLinked) {
        const ScopedLock lock(vertexLock);
        if (envelopeRedLinked == redLinked && envelopeBlueLinked == blueLinked) {
            return;
        }

        envelopeRedLinked = redLinked;
        envelopeBlueLinked = blueLinked;

        if (kind == NodeKind::Envelope && positioner != nullptr) {
            updateSelectionFrames();
            Panel2D::repaint();
        }
    }

    void setControlValues(bool isEnabled, float firstValue, float secondValue, float thirdValue, int menuId) {
        enabled = isEnabled;
        controlA = firstValue;
        controlB = secondValue;
        controlC = thirdValue;
        selectedMenuId = menuId;

        if (kind == NodeKind::Envelope) {
            applyEnvelopeMorphPosition(false);
        }
    }

    bool doCreateVertex() override {
        return addNewCube(0.f, state.currentMouse.x, state.currentMouse.y, 0.f);
    }

    void mouseDoubleClick(const MouseEvent& event) override {
        updateCurrentMouseFromLocalPosition(event.getPosition());
        doCreateVertex();
    }

    bool locateClosestElement() override {
        if (kind == NodeKind::Envelope) {
            return Interactor2D::locateClosestElement();
        }

        state.currentIcpt = -1;
        state.currentFreeVert = -1;
        state.currentCube = nullptr;
        return Interactor::locateClosestElement();
    }

    void setExtraElements(float x) override {
        if (kind != NodeKind::Envelope) {
            Interactor2D::setExtraElements(x);
            return;
        }

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
        if (kind == NodeKind::Envelope) {
            ignoreUnused(startTime);
            return addEnvelopeCubeAt(x, y, curve);
        }

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

    void transferLineProperties(VertCube const* from, VertCube* to1, VertCube* to2) override {
        Interactor::transferLineProperties(from, to1, to2);

        if (kind != NodeKind::Envelope || from == nullptr) {
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

        jassert(envelopeMesh != nullptr);
        transferMarker(envelopeMesh->loopCubes);
        transferMarker(envelopeMesh->sustainCubes);
    }

    Array<Vertex*> getVerticesToMove(VertCube* cube, Vertex* startVertex) override {
        if (kind != NodeKind::Envelope || cube == nullptr || startVertex == nullptr) {
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
        if (kind != NodeKind::Envelope) {
            Interactor2D::doReshapeCurve(event);
            return;
        }

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
        const float diffY = (state.currentMouse.y - state.lastMouse.y) / sqrtf(panel->getZoomPanel()->rect.h);
        const float diff = diffY / (0.1f + curves[getStateValue(CurrentCurve)].tp.scaleY);
        const int pole = curves[getStateValue(CurrentCurve)].tp.ypole;

        for (auto* vertex : movingVerts) {
            float& weight = vertex->values[Vertex::Curve];
            weight += diff * pole;
            NumberUtils::constrain(weight, 0.f, 1.f);
        }

        listeners.call(&InteractorListener::selectionChanged, getMesh(), state.selectedFrame);
        flag(DidMeshChange) |= diff != 0.f;
    }

    void doExtraMouseDrag(const MouseEvent& event) override {
        Interactor2D::doExtraMouseDrag(event);

        if (kind == NodeKind::Envelope
                && (actionIs(PanelState::DraggingVertex)
                        || actionIs(PanelState::ReshapingCurve)
                        || actionIs(PanelState::DraggingCorner))
                && synchronizeEnvelopeLoopSeamForCurrentEdit()) {
            envRasterizer.updateWaveform(&mesh, 0.f);
            state.flags[PanelState::DidMeshChange] = true;
            Panel2D::repaint();
        }
    }

    void setMovingVertsFromSelected() override {
        if (kind != NodeKind::Envelope) {
            Interactor::setMovingVertsFromSelected();
            return;
        }

        state.selectedFrame.clear();
        vector<Vertex*>& selected = getSelected();

        for (auto* vertex : selected) {
            VertCube* cube = state.currentCube != nullptr && vertex == state.currentVertex
                    ? state.currentCube
                    : closestEnvelopeCubeFor(vertex);
            addToArray(getVerticesToMove(cube, vertex), state.selectedFrame);
        }
    }

    void refreshRasterizer() {
        if (kind == NodeKind::Envelope) {
            envRasterizer.setMorphPosition(envelopeMorphPosition());
            envRasterizer.updateWaveform(&mesh, 0.f);
            validateEnvelopeMarkers();
            if (synchronizeEnvelopeLoopSeamForCurrentEdit()) {
                envRasterizer.updateWaveform(&mesh, 0.f);
                validateEnvelopeMarkers();
            }
        } else {
            fxRasterizer.updateGeometry();
            fxRasterizer.updateWaveform();
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
        if (kind == NodeKind::Envelope) {
            drawEnvelopeBackground();
        } else if (kind == NodeKind::GuideCurve) {
            drawGuideBackground();
        } else if (kind == NodeKind::ImpulseResponse) {
            drawIrBackground();
        }
    }

    void postCurveDraw() override {
        if (kind == NodeKind::Envelope) {
            drawEnvelopeBounds();
        } else if (kind == NodeKind::Waveshaper) {
            drawWaveshaperBounds();
        } else if (kind == NodeKind::GuideCurve) {
            drawGuideBounds();
        } else if (kind == NodeKind::ImpulseResponse) {
            drawIrBounds();
        }
    }

    void drawInterceptLines() override {}

    var automationState() const {
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
        Array<var> vertexParameters;
        for (const auto& parameter : selectedVertexParameters()) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("id", parameter.id);
            encoded->setProperty("value", parameter.value);
            vertexParameters.add(encoded);
        }
        root->setProperty("selectedVertexParameters", vertexParameters);

        if (kind == NodeKind::Envelope) {
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
        } else {
            auto snapshot = const_cast<FXRasterizer&>(fxRasterizer).snapshotView();
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
        }

        return var(root);
    }

    std::vector<TrimeshVertexParameter> selectedVertexParameters() const {
        const auto& selected = const_cast<EffectPanel*>(this)->getSelected();
        const Vertex* vertex = !selected.empty() ? selected.front() : firstEditableVertex();

        if (vertex == nullptr) {
            return {};
        }

        if (kind == NodeKind::Envelope) {
            return {
                    { "vertex.red", "red", vertex->values[Vertex::Red], 0.f, 1.f },
                    { "vertex.blue", "blue", vertex->values[Vertex::Blue], 0.f, 1.f },
                    { "vertex.phase", "phase", vertex->values[Vertex::Phase], 0.f, 1.5f },
                    { "vertex.amp", "amp", vertex->values[Vertex::Amp], 0.f, 1.f },
                    { "vertex.curve", "curve", vertex->values[Vertex::Curve], 0.f, 1.f }
            };
        }

        return {
                { "vertex.phase", "phase", vertex->values[Vertex::Phase], 0.f, 1.f },
                { "vertex.amp", "amp", vertex->values[Vertex::Amp], 0.f, 1.f },
                { "vertex.curve", "curve", vertex->values[Vertex::Curve], 0.f, 1.f }
        };
    }

    bool setSelectedVertexParameter(const String& parameterId, float normalizedValue) {
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

        if (kind == NodeKind::Envelope
                && (dimension == Vertex::Phase || dimension == Vertex::Amp || dimension == Vertex::Curve)) {
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
        if (kind != NodeKind::Envelope) {
            return nullptr;
        }

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

    bool selectedEnvelopeMarkerState(bool loopMarker) const {
        VertCube* cube = selectedEnvelopeCube();

        if (cube == nullptr) {
            return false;
        }

        if (envelopeMesh == nullptr) {
            return false;
        }

        const auto& markers = loopMarker ? envelopeMesh->loopCubes : envelopeMesh->sustainCubes;
        return markers.find(cube) != markers.end();
    }

    void toggleSelectedEnvelopeMarker(bool loopMarker) {
        VertCube* cube = selectedEnvelopeCube();

        if (cube == nullptr) {
            return;
        }

        if (envelopeMesh == nullptr) {
            return;
        }

        auto& markers = loopMarker ? envelopeMesh->loopCubes : envelopeMesh->sustainCubes;
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

    void drawCurvesAndSurfaces() override {
        if (kind == NodeKind::Envelope) {
            drawEnvelopeCurveAndSections();
            drawDepthLinesAndVerts();
            return;
        }

        if (kind == NodeKind::Waveshaper
                || kind == NodeKind::GuideCurve
                || kind == NodeKind::ImpulseResponse) {
            drawVertexSegments();
        }

        Panel2D::drawCurvesAndSurfaces();
    }

private:
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
        if (kind != NodeKind::Envelope) {
            return;
        }

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

        jassert(envelopeMesh != nullptr);
        auto* cube = new VertCube(envelopeMesh);
        cube->initVerts(position);

        for (int i = 0; i < (int) VertCube::numVerts; ++i) {
            Vertex* vertex = cube->getVertex(i);
            vertex->values[Vertex::Phase] = x;
            vertex->values[Vertex::Amp] = y;

            if (curve >= 0.f) {
                vertex->values[Vertex::Curve] = curve;
            }
        }

        envelopeMesh->addCube(cube);
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

        if (kind == NodeKind::Envelope && parameterId.endsWithIgnoreCase(".phase")) {
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

    float paddingForKind() const {
        if (kind == NodeKind::Envelope) {
            return 0.f;
        }

        if (kind == NodeKind::GuideCurve) {
            return kGuidePadding;
        }

        if (kind == NodeKind::Waveshaper) {
            return kWaveshaperPadding;
        }

        return kIrPadding;
    }

    void updateZoomBounds(bool resetView) {
        if (zoomPanel == nullptr) {
            return;
        }

        const float padding = paddingForKind();
        const float xMinimum = 0.5f * padding;
        const float xMaximum = 1.f - 0.5f * padding;
        const float yMinimum = kind == NodeKind::Waveshaper ? 0.5f * padding : 0.f;
        const float yMaximum = kind == NodeKind::Waveshaper ? 1.f - 0.5f * padding : 1.f;

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

        if (kind == NodeKind::Envelope) {
            float maxX = kind == NodeKind::Envelope ? 1.5f : 1.f;
            MorphPosition position;

            for (VertCube* cube : mesh.getCubes()) {
                if (cube != nullptr) {
                    jassert(envelopeMesh != nullptr);
                    maxX = jmax(maxX, envelopeMesh->getPositionOfCubeAt(cube, position));
                }
            }

            zoomPanel->rect.xMinimum = 0.f;
            zoomPanel->rect.xMaximum = maxX;

            if (resetView) {
                zoomPanel->rect.x = 0.f;
                zoomPanel->rect.w = maxX;
            }
        }

        panel->constrainZoom();
        updateEnvelopeBackgroundGrid();
    }

    void updateEnvelopeBackgroundGrid() {
        if (kind != NodeKind::Envelope) {
            return;
        }

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

        if (envelopeMesh != nullptr) {
            removeMissingCubes(envelopeMesh->loopCubes);
            removeMissingCubes(envelopeMesh->sustainCubes);
        }

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
                        && envelopeMesh != nullptr
                        && envelopeMesh->loopCubes.find(cube) != envelopeMesh->loopCubes.end()
                        && i > sustIdx - EnvRasterizer::loopMinSizeIcpts) {
                    envelopeMesh->loopCubes.erase(cube);
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

    void drawGuideBackground() {
        const float padding = kGuidePadding;
        const int left = 0;
        const int right = getWidth();
        const int innerLeft = sx(padding);
        const int innerRight = sx(1.f - padding);
        const int bottom = 0;
        const int top = getHeight();

        gfx->setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
        gfx->fillRect(left, top, innerLeft, bottom, false);
        gfx->fillRect(right, top, innerRight, bottom, false);

        gfx->disableSmoothing();
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.32f);
        gfx->drawLine(innerLeft, top, innerLeft, bottom, false);
        gfx->drawLine(innerRight, top, innerRight, bottom, false);
        gfx->enableSmoothing();

        const float rectLeft = padding;
        const float rectRight = 1.f - padding;
        const float rectSize = 1.f - 2.f * padding;

        gfx->setCurrentColour(0.7f, 0.55f, 0.18f, 0.17f);
        gfx->fillRect(rectLeft, 0.5f + 0.5f * controlA, rectRight, 0.5f - 0.5f * controlA, true);

        gfx->setCurrentColour(0.7f, 0.08f, 0.5f, 0.17f);
        gfx->fillRect(rectLeft, 0.5f + 0.5f * controlB, rectRight, 0.5f - 0.5f * controlB, true);

        gfx->setCurrentColour(0.3f, 0.6f, 0.9f, 0.17f);
        gfx->fillRect(
                0.5f - 0.5f * rectSize * controlC,
                0.f,
                0.5f + 0.5f * rectSize * controlC,
                1.f,
                true);
    }

    void drawIrBackground() {
        const int left = 0;
        const int bottom = 0;
        const int top = getHeight();
        const int innerLeft = sx(kIrPadding);

        gfx->setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
        gfx->fillRect(left, top, innerLeft, bottom, false);
        gfx->disableSmoothing();
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.28f);
        gfx->drawLine(innerLeft, top, innerLeft, bottom, false);
        gfx->enableSmoothing();

    }

    void drawWaveshaperBounds() {
        const int left = 0;
        const int right = getWidth();
        const int innerRight = sx(1.f - kWaveshaperPadding);
        const int innerLeft = sx(kWaveshaperPadding);
        const int bottom = 0;
        const int low = sy(1.f - kWaveshaperPadding);
        const int high = sy(kWaveshaperPadding);
        const int top = getHeight();

        gfx->setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
        gfx->fillRect(innerLeft, top, innerRight, high, false);
        gfx->fillRect(innerLeft, bottom, innerRight, low, false);
        gfx->fillRect(left, top, innerLeft, bottom, false);
        gfx->fillRect(right, top, innerRight, bottom, false);

        gfx->disableSmoothing();
        gfx->setCurrentLineWidth(1.f);
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.42f);
        gfx->drawRect(innerLeft, high, innerRight, low, false);
        gfx->enableSmoothing();
    }

    void drawGuideBounds() {
        const int innerLeft = sx(kGuidePadding);
        const int innerRight = sx(1.f - kGuidePadding);

        gfx->disableSmoothing();
        gfx->setCurrentLineWidth(1.f);
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.42f);
        gfx->drawLine(innerLeft, getHeight() - 1, innerRight, getHeight() - 1, false);
        gfx->drawLine(innerLeft, 1, innerRight, 1, false);
        gfx->drawLine(innerLeft, getHeight() - 1, innerLeft, 1, false);
        gfx->drawLine(innerRight, getHeight() - 1, innerRight, 1, false);
        gfx->enableSmoothing();
    }

    void drawVertexSegments() {
        std::vector<Vertex*> vertices = mesh.getVerts();

        if (vertices.size() < 2) {
            return;
        }

        std::sort(vertices.begin(), vertices.end(), [](const Vertex* lhs, const Vertex* rhs) {
            return lhs->values[Vertex::Phase] < rhs->values[Vertex::Phase];
        });

        gfx->enableSmoothing();
        gfx->setCurrentLineWidth(1.f);
        const float alpha = kind == NodeKind::Waveshaper ? 0.16f : 0.24f;
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, alpha);

        for (int i = 1; i < (int) vertices.size(); ++i) {
            gfx->drawLine(
                    sx(vertices[(size_t) i - 1]->values[Vertex::Phase]),
                    sy(vertices[(size_t) i - 1]->values[Vertex::Amp]),
                    sx(vertices[(size_t) i]->values[Vertex::Phase]),
                    sy(vertices[(size_t) i]->values[Vertex::Amp]),
                    false);
        }

        gfx->setCurrentLineWidth(1.f);
    }

    void drawIrBounds() {
        const int innerLeft = sx(kIrPadding);
        const int right = getWidth() - 1;
        const int bottom = 1;
        const int top = getHeight() - 1;

        gfx->disableSmoothing();
        gfx->setCurrentLineWidth(1.f);
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.42f);
        gfx->drawLine(innerLeft, top, right, top, false);
        gfx->drawLine(innerLeft, bottom, right, bottom, false);
        gfx->drawLine(innerLeft, top, innerLeft, bottom, false);
        gfx->drawLine(right, top, right, bottom, false);
        gfx->enableSmoothing();
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
        if (kind != NodeKind::Envelope) {
            return false;
        }

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
                if (envelopeMesh != nullptr && isMarked(envelopeMesh->loopCubes, owner)) {
                    movingLoopLine = true;
                    sourceVertex = vertex;
                }
                if (envelopeMesh != nullptr && isMarked(envelopeMesh->sustainCubes, owner)) {
                    movingSustainLine = true;
                    sourceVertex = vertex;
                }
            }
        }

        if (selected.empty()) {
            movingLoopLine = envelopeMesh != nullptr
                    && isMarked(envelopeMesh->loopCubes, state.currentCube);
            movingSustainLine = envelopeMesh != nullptr
                    && isMarked(envelopeMesh->sustainCubes, state.currentCube);
        }

        if (movingLoopLine && movingSustainLine) {
            return false;
        }

        if (!movingLoopLine && !movingSustainLine) {
            return false;
        }

        return synchronizeEnvelopeLoopSeamFrom(sourceVertex, movingLoopLine);
    }

    FXRasterizer fxRasterizer;
    EnvRasterizer envRasterizer;
    TrimeshPanelEnvironment& environment;
    Mesh& mesh;
    EnvelopeMesh* envelopeMesh {};
    NodeKind kind { NodeKind::Waveshaper };
    bool enabled { true };
    float controlA { 0.5f };
    float controlB { 0.5f };
    float controlC { 0.5f };
    int selectedMenuId {};
    bool envelopeLogarithmic {};
    bool envelopeRedLinked { true };
    bool envelopeBlueLinked { true };
};

Effect2DPanelBridge::Effect2DPanelBridge(NodeKind nodeKind) :
        kind    (nodeKind)
    ,   adapter (nodeKind == NodeKind::Envelope
                ? decltype(adapter)(std::in_place_type<EnvelopePanelAdapter>)
                : decltype(adapter)(std::in_place_type<FlatCurvePanelAdapter>, nodeKind)) {
    jassert(isEffect2DNode(kind));

    panel = std::make_unique<EffectPanel>(
            &environment.services().getRepo(),
            "CycleV2Effect2DPanel",
            environment.services(),
            adapterMesh(),
            envelopeAdapter() != nullptr ? &envelopeAdapter()->mesh() : nullptr);
    panel->setNodeKind(kind);
    host = std::make_unique<CurvePanelHost>(
            *panel,
            [this](Component* component) {
                panel->initWithHost(component);
                panel->stopTimer();
            },
            [this](bool resetView) { panel->updateZoomBounds(resetView); },
            [this] {
                applyPanelSettings();
                panel->refreshRasterizer();
            },
            [this] { return notifyMeshEdited(); },
            [this] { synchronizeModelSelection(); });

    if (kind != NodeKind::Envelope) {
        initialiseMesh();
    }
}

Effect2DPanelBridge::~Effect2DPanelBridge() {
    releaseSharedGlResources();
}

void Effect2DPanelBridge::syncFromNode(const Node& node) {
    if (node.kind != kind) {
        return;
    }
    const bool didSync = std::visit([&node](auto& typedAdapter) {
        return typedAdapter.syncFromNode(node);
    }, adapter);
    if (!didSync) {
        return;
    }

    std::visit([this](const auto& typedAdapter) {
        lastSyncedNodeId = typedAdapter.lastNodeId();
        lastSyncedModelSnapshot = typedAdapter.lastModelSnapshot();
        lastSyncedMeshState = typedAdapter.lastMeshState();
    }, adapter);
    publicationRevision = jmax<uint64_t>(1, (uint64_t) parameterValueForNode(
            node, CurveNodeModelCodec::revisionParameterId(), "1").getLargeIntValue());
    applyPanelSettings();
    panel->refreshRasterizer();
}

Component* Effect2DPanelBridge::getPanelHostComponent() {
    initialiseMesh();
    return host->component();
}

Component* Effect2DPanelBridge::getPanelHostComponentIfCreated() {
    return host->componentIfCreated();
}

void Effect2DPanelBridge::setPanelHostCallbacks(
        std::function<void()> repaintCallback,
        std::function<void(const MouseCursor&)> cursorCallback) {
    host->setCallbacks(std::move(repaintCallback), std::move(cursorCallback));
}

void Effect2DPanelBridge::setMeshEditedCallback(std::function<void()> callback) {
    meshEditedCallback = std::move(callback);
}

void Effect2DPanelBridge::setControlValues(
        bool enabled,
        float firstValue,
        float secondValue,
        float thirdValue,
        int menuId) {
    const ControlState next { enabled, firstValue, secondValue, thirdValue, menuId };
    const bool changed = controls.enabled != next.enabled
            || controls.first != next.first
            || controls.second != next.second
            || controls.third != next.third
            || controls.menuId != next.menuId;
    controls = next;
    if (auto* envelope = envelopeAdapter()) {
        envelope->setMorph(firstValue, secondValue);
    }
    if (changed) {
        ++publicationRevision;
    }
    applyPanelSettings();
}

void Effect2DPanelBridge::setEnvelopeLogarithmic(bool shouldUseLogarithmicScale) {
    auto* envelope = envelopeAdapter();
    if (envelope == nullptr) {
        return;
    }
    if (envelope->logarithmic() != shouldUseLogarithmicScale) {
        ++publicationRevision;
    }
    envelope->setLogarithmic(shouldUseLogarithmicScale);

    if (panel != nullptr) {
        panel->setEnvelopeLogarithmic(shouldUseLogarithmicScale);
    }
}

void Effect2DPanelBridge::setEnvelopeAxisLinks(bool redLinked, bool blueLinked) {
    auto* envelope = envelopeAdapter();
    if (envelope == nullptr) {
        return;
    }
    if (envelope->redLinked() != redLinked || envelope->blueLinked() != blueLinked) {
        ++publicationRevision;
    }
    envelope->setAxisLinks(redLinked, blueLinked);
    panel->setEnvelopeAxisLinks(redLinked, blueLinked);
}

void Effect2DPanelBridge::renderPanel(
        Rectangle<float> bounds,
        Rectangle<float> clipBounds,
        float scaleFactor) {
    host->render(bounds, clipBounds, scaleFactor);
}

void Effect2DPanelBridge::renderPreviewSnapshot(Rectangle<float> bounds, float scaleFactor) {
    host->renderPreview(bounds, scaleFactor);
}

bool Effect2DPanelBridge::paintExpandedSnapshot(Graphics& g, Rectangle<float> bounds) const {
    return host->paintExpandedSnapshot(g, bounds);
}

bool Effect2DPanelBridge::paintPreviewSnapshot(Graphics& g, Rectangle<float> bounds) const {
    return host->paintPreviewSnapshot(g, bounds);
}

void Effect2DPanelBridge::initialiseSharedGlResources() {
    host->component();
}

void Effect2DPanelBridge::releaseSharedGlResources() {
    host->releaseSharedGlResources();
}

void Effect2DPanelBridge::applyPanelSettings() {
    if (panel != nullptr) {
        panel->setControlValues(
                controls.enabled,
                controls.first,
                controls.second,
                controls.third,
                controls.menuId);
        if (const auto* envelope = envelopeAdapter()) {
            panel->setEnvelopeLogarithmic(envelope->logarithmic());
            panel->setEnvelopeAxisLinks(envelope->redLinked(), envelope->blueLinked());
        }
    }
}

int Effect2DPanelBridge::vertexCountForAutomation() const {
    return adapterMesh().getNumVerts();
}

var Effect2DPanelBridge::automationState() const {
    var state = panel != nullptr ? panel->automationState() : var(new DynamicObject());
    if (auto* object = state.getDynamicObject()) {
        object->setProperty("modelRevision", (int64) publicationRevision);
        object->setProperty("domainModel", kind == NodeKind::Envelope ? "envelope" : "flatCurve");
    }
    return state;
}

std::vector<Effect2DPanelBridge::PreviewVertex> Effect2DPanelBridge::previewVertices() {
    return std::visit([](auto& typedAdapter) {
        return typedAdapter.previewVertices();
    }, adapter);
}

String Effect2DPanelBridge::serializedMeshState() {
    return std::visit([](auto& typedAdapter) {
        return typedAdapter.serializedMeshState();
    }, adapter);
}

String Effect2DPanelBridge::serializedModelSnapshot() {
    Vertex* selectedVertex = panel != nullptr ? panel->selectedFlatVertexForModel() : nullptr;
    VertCube* selectedCube = panel != nullptr ? panel->selectedEnvelopeCubeForModel() : nullptr;
    const String snapshot = flatAdapter() != nullptr
            ? flatAdapter()->serializedModelSnapshot(selectedVertex, publicationRevision)
            : envelopeAdapter()->serializedModelSnapshot(selectedCube, publicationRevision);
    if (snapshot.isEmpty() && panel != nullptr) {
        panel->clearInteractionState();
        panel->refreshRasterizer();
    }
    lastSyncedModelSnapshot = snapshot;
    return lastSyncedModelSnapshot;
}

std::vector<TrimeshVertexParameter> Effect2DPanelBridge::selectedVertexParameters() const {
    return panel != nullptr ? panel->selectedVertexParameters() : std::vector<TrimeshVertexParameter>();
}

bool Effect2DPanelBridge::setSelectedVertexParameter(const String& parameterId, float normalizedValue) {
    if (panel == nullptr || !panel->setSelectedVertexParameter(parameterId, normalizedValue)) {
        return false;
    }

    notifyMeshEdited();
    return true;
}

bool Effect2DPanelBridge::selectedEnvelopeMarkerState(bool loopMarker) const {
    return panel != nullptr && panel->selectedEnvelopeMarkerState(loopMarker);
}

void Effect2DPanelBridge::toggleSelectedEnvelopeMarker(bool loopMarker) {
    if (panel == nullptr) {
        return;
    }

    panel->toggleSelectedEnvelopeMarker(loopMarker);
    notifyMeshEdited();
}

void Effect2DPanelBridge::initialiseMesh() {
    std::visit([](auto& typedAdapter) {
        typedAdapter.initialiseDefaultMesh();
    }, adapter);

    panel->refreshRasterizer();

    if (kind == NodeKind::Envelope) {
        panel->updateZoomBounds(true);
    }
}

bool Effect2DPanelBridge::notifyMeshEdited() {
    if (meshEditedCallback == nullptr) {
        return false;
    }

    const bool changed = std::visit([](auto& typedAdapter) {
        return typedAdapter.registerMeshEdit();
    }, adapter);
    if (!changed) {
        return false;
    }

    std::visit([this](const auto& typedAdapter) {
        lastSyncedMeshState = typedAdapter.lastMeshState();
    }, adapter);
    ++publicationRevision;
    meshEditedCallback();
    return true;
}

void Effect2DPanelBridge::synchronizeModelSelection() {
    if (panel == nullptr) {
        return;
    }
    if (auto* flat = flatAdapter()) {
        flat->serializedModelSnapshot(
                panel->selectedFlatVertexForModel(), publicationRevision);
    } else {
        envelopeAdapter()->serializedModelSnapshot(
                panel->selectedEnvelopeCubeForModel(), publicationRevision);
    }
}

Mesh& Effect2DPanelBridge::adapterMesh() {
    return std::visit([](auto& typedAdapter) -> Mesh& {
        return typedAdapter.mesh();
    }, adapter);
}

const Mesh& Effect2DPanelBridge::adapterMesh() const {
    return std::visit([](const auto& typedAdapter) -> const Mesh& {
        return typedAdapter.mesh();
    }, adapter);
}

FlatCurvePanelAdapter* Effect2DPanelBridge::flatAdapter() {
    return std::get_if<FlatCurvePanelAdapter>(&adapter);
}

const FlatCurvePanelAdapter* Effect2DPanelBridge::flatAdapter() const {
    return std::get_if<FlatCurvePanelAdapter>(&adapter);
}

EnvelopePanelAdapter* Effect2DPanelBridge::envelopeAdapter() {
    return std::get_if<EnvelopePanelAdapter>(&adapter);
}

const EnvelopePanelAdapter* Effect2DPanelBridge::envelopeAdapter() const {
    return std::get_if<EnvelopePanelAdapter>(&adapter);
}

}

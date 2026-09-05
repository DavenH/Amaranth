#include "Nodes/Curve/Panel/CurvePanelController.h"

#include <optional>

#include "Graph/NodeParameterMap.h"
#include "Nodes/Curve/Panel/ConcreteCurvePanels.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Curve/Panel/CurvePanelInfrastructure.h"
#include "Nodes/Envelope/Editor/EnvelopePanelAdapter.h"
#include "Nodes/Curve/Panel/FlatCurvePanelAdapter.h"
#include "Nodes/Envelope/EnvelopePurpose.h"
#include "Nodes/ImpulseResponse/ImpulseResponseAnalysis.h"
#include "Nodes/Guide/GuideCurvePreparation.h"
#include "Nodes/Guide/GuideHeatmapAsset.h"

namespace CycleV2 {

namespace {

class CurvePanelControllerBase : public CurvePanelController,
                                 private CurvePanelHostDelegate {
public:
    ~CurvePanelControllerBase() override {
        releaseSharedGlResources();
    }

    Component* panelHostComponent() override {
        initialiseDefaultModel();
        return host->component();
    }

    Component* panelHostComponentIfCreated() override {
        return host->componentIfCreated();
    }

    void setDelegate(CurvePanelControllerDelegate* nextDelegate) override {
        controllerDelegate = nextDelegate;
    }

    void setControlValues(
            bool enabled,
            float first,
            float second,
            float third,
            int menuId) override {
        const ControlState next { enabled, first, second, third, menuId };
        const bool changed = controls != next;
        controls = next;
        applyDomainControlValues(first, second, third);
        if (changed) {
            ++publicationRevision;
        }
        applyPanelSettings();
    }

    void render(
            Rectangle<float> bounds,
            Rectangle<float> clipBounds,
            float scaleFactor) override {
        host->render(bounds, clipBounds, scaleFactor);
    }

    void renderPreview(
            Rectangle<float> bounds,
            float scaleFactor,
            uint64_t presentationRevision) override {
        host->renderPreview(
                bounds,
                scaleFactor,
                preservesInteractivePreviewZoom(),
                publicationRevision,
                transientContentRevision,
                presentationRevision);
    }

    bool paintExpandedSnapshot(Graphics& graphics, Rectangle<float> bounds) const override {
        return host->paintExpandedSnapshot(graphics, bounds);
    }

    bool paintPreviewSnapshot(Graphics& graphics, Rectangle<float> bounds) const override {
        return host->paintPreviewSnapshot(graphics, bounds);
    }

    void releaseSharedGlResources() override {
        if (host != nullptr) {
            host->releaseSharedGlResources();
        }
    }

    int vertexCountForAutomation() const override {
        return modelMesh().getNumVerts();
    }

    var automationState() const override {
        var state = panel != nullptr ? panel->automationState() : var(new DynamicObject());
        if (auto* object = state.getDynamicObject()) {
            object->setProperty("modelRevision", (int64) publicationRevision);
            object->setProperty("domainModel", domainModelName());
            object->setProperty("curveResizeCursor", host->usesCursor(MouseCursor::UpDownResizeCursor));
            object->setProperty(
                    "previewPreservesInteractiveZoom",
                    preservesInteractivePreviewZoom());
            const auto previewCache = host->previewRenderDiagnostics();
            object->setProperty("previewRenderCacheHits", (int64) previewCache.hits);
            object->setProperty("previewRenderCacheMisses", (int64) previewCache.misses);
        }
        return state;
    }

    std::vector<CurvePanelGridLine> verticalMajorGridLines() const override {
        return panel != nullptr
                ? panel->verticalMajorGridLines()
                : std::vector<CurvePanelGridLine>();
    }

    NodeModelStatePtr prepareModelPublication(uint64_t currentRevision) override {
        publicationRevision = jmax(publicationRevision, currentRevision + 1);
        return modelPublication();
    }

    uint64_t modelRevision() const override {
        return publicationRevision;
    }

    uint64_t contentRevision() const override {
        return transientContentRevision;
    }

    std::vector<TrimeshVertexParameter> selectedVertexParameters() const override {
        return panel->selectedVertexParameters();
    }

    bool setSelectedVertexParameter(const String& parameterId, float value) override {
        if (!panel->setSelectedVertexParameter(parameterId, value)) {
            return false;
        }
        notifyMeshEdited();
        return true;
    }

protected:
    struct ControlState {
        bool enabled { true };
        float first { 0.5f };
        float second { 0.5f };
        float third { 0.5f };
        int menuId {};

        bool operator!=(const ControlState& other) const {
            return enabled != other.enabled
                || first != other.first
                || second != other.second
                || third != other.third
                || menuId != other.menuId;
        }
    };

    void initialiseHost() {
        host = std::make_unique<CurvePanelHost>(
                panel->hostedPanel(), static_cast<CurvePanelHostDelegate&>(*this));
    }

    void finishNodeSync(const Node& node) {
        publicationRevision = node.model != nullptr ? node.model->revision() : 1;
        applyPanelSettings();
        panel->refreshRasterizer();
    }

    virtual void initialiseDefaultModel() = 0;
    virtual bool registerMeshEdit() = 0;
    virtual void synchronizeSelection() = 0;
    virtual void applyDomainPanelSettings() {}
    virtual void applyDomainControlValues(float, float, float) {}
    virtual const Mesh& modelMesh() const = 0;
    virtual String domainModelName() const = 0;
    virtual bool preservesInteractivePreviewZoom() const { return false; }

    bool notifyMeshEdited() {
        if (controllerDelegate == nullptr || !registerMeshEdit()) {
            return false;
        }
        ++transientContentRevision;
        controllerDelegate->curvePanelControllerEdited();
        return true;
    }

    void applyPanelSettings() {
        panel->setControlValues(
                controls.enabled,
                controls.first,
                controls.second,
                controls.third,
                controls.menuId);
        applyDomainPanelSettings();
    }

    CurvePanelEnvironment environment;
    std::unique_ptr<CurvePanel> panel;
    std::unique_ptr<CurvePanelHost> host;
    ControlState controls;
    uint64_t publicationRevision { 1 };
    uint64_t transientContentRevision { 1 };

private:
    void initialiseCurvePanel(Component* component) override {
        panel->initWithHost(component);
    }

    void updateCurvePanelZoom(bool resetView) override {
        panel->updateZoomBounds(resetView);
    }

    void prepareCurvePanel() override {
        applyPanelSettings();
        panel->refreshRasterizer();
    }

    void beginEdit() override {
        editChanged = false;
        if (controllerDelegate != nullptr) {
            controllerDelegate->beginCurvePanelControllerEdit();
        }
    }

    void publishIntermediateRevision() override {
        editChanged = notifyMeshEdited() || editChanged;
    }

    void commitEdit() override {
        if (!editChanged) {
            synchronizeSelection();
        }
        if (controllerDelegate != nullptr) {
            controllerDelegate->commitCurvePanelControllerEdit();
        }
    }

    void synchronizeCurvePanelSelection() override {
        synchronizeSelection();
    }

    void repaintCurvePanel() override {
        if (controllerDelegate != nullptr) {
            controllerDelegate->repaintCurvePanelController();
        }
    }

    CurvePanelControllerDelegate* controllerDelegate {};
    bool editChanged {};
};

class FlatPanelController : public CurvePanelControllerBase {
public:
    explicit FlatPanelController(NodeKind kindToUse) :
            adapter(kindToUse) {
        panel = createFlatCurvePanel(
                kindToUse, &environment.services().getRepo(), adapter.mesh());
        initialiseHost();
        initialiseDefaultModel();
    }

    explicit FlatPanelController(bool guideResource) :
            adapter(guideResource) {
        panel = createGuideCurvePanel(
                &environment.services().getRepo(), adapter.mesh());
        initialiseHost();
        initialiseDefaultModel();
    }

    void syncFromNode(const Node& node) override {
        beforeNodeSync(node);
        if (!adapter.needsNodeSync(node)) {
            return;
        }
        auto& flatPanel = static_cast<FlatCurvePanelContract&>(*panel);
        panel->clearInteractionState();
        if (adapter.syncFromNode(node)) {
            finishNodeSync(node);
            flatPanel.restoreFlatSelection(adapter.selectedMeshVertex());
        } else {
            flatPanel.restoreFlatSelection(adapter.selectedMeshVertex());
        }
    }

    bool syncFromGuideResource(
            const GuideCurveResource& guide,
            const GuideHeatmapAssetPtr& heatmap) override {
        guideState = guide;
        guideHeatmap = heatmap;
        const bool modelChanged = adapter.syncFromGuideResource(guide);
        if (modelChanged) {
            auto& flatPanel = static_cast<FlatCurvePanelContract&>(*panel);
            panel->clearInteractionState();
            finishGuideSync(guide);
            flatPanel.restoreFlatSelection(adapter.selectedMeshVertex());
        }
        updateGuidePresentation(guide, heatmap);
        return modelChanged;
    }

    std::vector<CurvePreviewVertex> previewVertices() override {
        return adapter.previewVertices();
    }

    String serializedMeshState() override {
        return adapter.serializedMeshState();
    }

    NodeModelStatePtr modelPublication() override {
        auto& flatPanel = static_cast<FlatCurvePanelContract&>(*panel);
        const auto publication = adapter.modelPublication(
                flatPanel.selectedFlatVertexForModel(), publicationRevision);
        if (publication == nullptr) {
            panel->clearInteractionState();
            panel->refreshRasterizer();
        } else if (guideHeatmap != nullptr) {
            guideState.model = publication;
            updateGuidePresentation(guideState, guideHeatmap);
        }
        return publication;
    }

protected:
    virtual void beforeNodeSync(const Node&) {}

private:
    void updateGuidePresentation(
            const GuideCurveResource& guide,
            const GuideHeatmapAssetPtr& heatmap) {
        auto* presentation = dynamic_cast<GuideCurvePanelPresentation*>(panel.get());
        if (presentation == nullptr) {
            return;
        }
        if (heatmap == nullptr) {
            presentation->setHeatmapPresentation({}, {});
            return;
        }

        constexpr int resolution = 512;
        std::vector<float> path(resolution);
        std::vector<float> output(resolution);
        const bool prepared = GuideCurvePreparation::prepare(
                guide,
                heatmap.get(),
                Buffer<float>(path.data(), resolution),
                Buffer<float>(output.data(), resolution));
        if (!prepared) {
            presentation->setHeatmapPresentation({}, {});
            return;
        }
        Buffer<float>(output.data(), resolution).add(0.5f);
        presentation->setHeatmapPresentation(heatmap->image(), std::move(output));
    }

    void finishGuideSync(const GuideCurveResource& guide) {
        publicationRevision = guide.model != nullptr ? guide.model->revision() : 1;
        applyPanelSettings();
        panel->refreshRasterizer();
    }

    void initialiseDefaultModel() override {
        adapter.initialiseDefaultMesh();
        panel->refreshRasterizer();
    }

    bool registerMeshEdit() override {
        return adapter.registerMeshEdit();
    }

    void synchronizeSelection() override {
        auto& flatPanel = static_cast<FlatCurvePanelContract&>(*panel);
        adapter.modelPublication(
                flatPanel.selectedFlatVertexForModel(), publicationRevision);
    }

    const Mesh& modelMesh() const override {
        return adapter.mesh();
    }

    String domainModelName() const override {
        return "flatCurve";
    }

    FlatCurvePanelAdapter adapter;
    GuideCurveResource guideState;
    GuideHeatmapAssetPtr guideHeatmap;
};

class ImpulseResponsePanelController final :
        public FlatPanelController,
        public ImpulseResponseCurvePanelController {
public:
    ImpulseResponsePanelController() :
            FlatPanelController(NodeKind::ImpulseResponse) {}

    void setAudioResource(const AudioSampleResource* resource) override {
        if (audioResourcesEqual(audioResource, resource)) {
            return;
        }
        audioResource = resource != nullptr
                ? std::optional<AudioSampleResource>(*resource)
                : std::nullopt;
        ++audioResourceRevision;
        updateSource();
        updateAnalysis(controls.third);
    }

    void zoomToAttack() override {
        impulseResponsePanel().zoomToAttack();
    }

    void resetZoom() override {
        impulseResponsePanel().resetZoom();
    }

    var automationState() const override {
        var state = FlatPanelController::automationState();
        if (auto* object = state.getDynamicObject()) {
            object->setProperty("irSourceRevision", (int64) sourceRevision);
            object->setProperty("irAnalysisRevision", (int64) analysisRevision);
        }
        return state;
    }

protected:
    void beforeNodeSync(const Node& node) override {
        currentNode = node;
        updateSource();
        const NodeParameterMap parameterMap(node.parameters);
        updateAnalysis(parameterMap.floatValue("highPass", 0.5f));
    }

    void applyDomainControlValues(float, float, float highPass) override {
        updateAnalysis(highPass);
    }

private:
    ImpulseResponseCurvePanelContract& impulseResponsePanel() {
        return dynamic_cast<ImpulseResponseCurvePanelContract&>(*panel);
    }

    void updateSource() {
        if (!currentNode.has_value()) {
            return;
        }
        const String signature = sourceSignature();
        if (signature == preparedSourceSignature) {
            return;
        }
        auto prepared = prepareImpulseResponseSource(
                currentNode->parameters,
                currentNode->model,
                audioResource.has_value() ? &*audioResource : nullptr);
        preparedSourceSignature = signature;
        ++sourceRevision;
        preparedAnalysisSignature = {};
        if (!prepared.has_value()) {
            source = nullptr;
            static_cast<FlatCurvePanelContract&>(*panel)
                    .setImpulseResponseAnalysis(nullptr);
            ++analysisRevision;
            return;
        }
        source = std::make_shared<const ImpulseResponseSource>(std::move(*prepared));
    }

    void updateAnalysis(float highPass) {
        if (source == nullptr) {
            return;
        }
        const String signature = String((int64) sourceRevision)
                + ":" + String(highPass, 9);
        if (signature == preparedAnalysisSignature) {
            return;
        }
        auto analysis = prepareImpulseResponseAnalysis(*source, highPass);
        static_cast<FlatCurvePanelContract&>(*panel).setImpulseResponseAnalysis(
                std::make_shared<const ImpulseResponseAnalysis>(std::move(analysis)));
        preparedAnalysisSignature = signature;
        ++analysisRevision;
    }

    String sourceSignature() const {
        const NodeParameterMap parameterMap(currentNode->parameters);
        const uint64_t modelRevision = currentNode->model != nullptr
                ? currentNode->model->revision()
                : 0;
        return String(parameterMap.floatValue("size", 0.5f), 9)
                + ":" + String((int64) modelRevision)
                + ":" + String((int64) audioResourceRevision);
    }

    static bool audioResourcesEqual(
            const std::optional<AudioSampleResource>& current,
            const AudioSampleResource* next) {
        if (!current.has_value() || next == nullptr) {
            return !current.has_value() && next == nullptr;
        }
        return current->id == next->id
                && current->name == next->name
                && current->sampleRate == next->sampleRate
                && current->samples == next->samples;
    }

    std::optional<Node> currentNode;
    std::optional<AudioSampleResource> audioResource;
    std::shared_ptr<const ImpulseResponseSource> source;
    String preparedSourceSignature;
    String preparedAnalysisSignature;
    uint64_t audioResourceRevision {};
    uint64_t sourceRevision {};
    uint64_t analysisRevision {};
};

class EnvelopePanelController final : public CurvePanelControllerBase,
                                      public EnvelopeCurvePanelController {
public:
    EnvelopePanelController() {
        panel = createEnvelopeCurvePanel(
                &environment.services().getRepo(), environment.services(), adapter.mesh());
        initialiseHost();
    }

    void syncFromNode(const Node& node) override {
        const EnvelopePurpose purpose = envelopePurposeFor(node);
        auto& typedPanel = envelopePanel();
        typedPanel.setEnvelopeBipolar(purpose == EnvelopePurpose::Pitch);
        if (!adapter.needsNodeSync(node)) {
            return;
        }

        panel->clearInteractionState();
        if (adapter.syncFromNode(node)) {
            finishNodeSync(node);
            typedPanel.restoreEnvelopeSelection(adapter.selectedMeshCube());
            if (purpose == EnvelopePurpose::Pitch) {
                typedPanel.fitEnvelopeVerticalRange();
            }
        } else {
            typedPanel.restoreEnvelopeSelection(adapter.selectedMeshCube());
        }
    }

    void setBipolar(bool bipolar) override {
        envelopePanel().setEnvelopeBipolar(bipolar);
    }

    void setLogarithmic(bool logarithmic) override {
        if (adapter.logarithmic() != logarithmic) {
            ++publicationRevision;
        }
        adapter.setLogarithmic(logarithmic);
        envelopePanel().setEnvelopeLogarithmic(logarithmic);
    }

    void setAxisLinks(bool redLinked, bool blueLinked) override {
        if (adapter.redLinked() != redLinked || adapter.blueLinked() != blueLinked) {
            ++publicationRevision;
        }
        adapter.setAxisLinks(redLinked, blueLinked);
        envelopePanel().setEnvelopeAxisLinks(redLinked, blueLinked);
    }

    void fitVerticalRange() override {
        envelopePanel().fitEnvelopeVerticalRange();
    }

    void resetVerticalRange() override {
        envelopePanel().resetEnvelopeVerticalRange();
    }

    bool hasSingleSelectedVertex() override {
        return envelopePanel().hasSingleSelectedEnvelopeVertex();
    }

    bool selectedMarkerState(bool loopMarker) const override {
        return envelopePanel().selectedEnvelopeMarkerState(loopMarker);
    }

    void toggleSelectedMarker(bool loopMarker) override {
        envelopePanel().toggleSelectedEnvelopeMarker(loopMarker);
        notifyMeshEdited();
    }

    std::vector<CurvePreviewVertex> previewVertices() override {
        return adapter.previewVertices();
    }

    String serializedMeshState() override {
        return adapter.serializedMeshState();
    }

    NodeModelStatePtr modelPublication() override {
        const auto publication = adapter.modelPublication(
                envelopePanel().selectedEnvelopeCubeForModel(), publicationRevision);
        if (publication == nullptr) {
            panel->clearInteractionState();
            panel->refreshRasterizer();
        }
        return publication;
    }

private:
    void initialiseDefaultModel() override {
        adapter.initialiseDefaultMesh();
        panel->refreshRasterizer();
        panel->updateZoomBounds(true);
    }

    bool registerMeshEdit() override {
        return adapter.registerMeshEdit();
    }

    void synchronizeSelection() override {
        adapter.modelPublication(
                envelopePanel().selectedEnvelopeCubeForModel(), publicationRevision);
    }

    void applyDomainPanelSettings() override {
        envelopePanel().setEnvelopeLogarithmic(adapter.logarithmic());
        envelopePanel().setEnvelopeAxisLinks(adapter.redLinked(), adapter.blueLinked());
    }

    void applyDomainControlValues(float red, float blue, float) override {
        adapter.setMorph(red, blue);
    }

    const Mesh& modelMesh() const override {
        return adapter.mesh();
    }

    String domainModelName() const override {
        return "envelope";
    }

    bool preservesInteractivePreviewZoom() const override {
        return true;
    }

    EnvelopeCurvePanelContract& envelopePanel() {
        return static_cast<EnvelopeCurvePanelContract&>(*panel);
    }

    const EnvelopeCurvePanelContract& envelopePanel() const {
        return static_cast<const EnvelopeCurvePanelContract&>(*panel);
    }

    EnvelopePanelAdapter adapter;
};

}

std::unique_ptr<CurvePanelController> createCurvePanelController(NodeKind kind) {
    if (kind == NodeKind::Envelope) {
        return std::make_unique<EnvelopePanelController>();
    }
    if (kind == NodeKind::ImpulseResponse) {
        return std::make_unique<ImpulseResponsePanelController>();
    }
    if (kind == NodeKind::Waveshaper) {
        return std::make_unique<FlatPanelController>(kind);
    }
    return nullptr;
}

std::unique_ptr<CurvePanelController> createGuideCurvePanelController() {
    return std::make_unique<FlatPanelController>(true);
}

}

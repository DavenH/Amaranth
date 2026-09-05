#pragma once

#include <JuceHeader.h>
#include <UI/Panels/PanelHostContext.h>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

#include "UI/RenderInvalidationAccumulator.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelEnvironment.h"

class CommonGL;
class GLPanelRenderer;
class Panel;

namespace CycleV2 {

std::optional<juce::Rectangle<int>> curvePanelFramebufferReadBounds(
        juce::Rectangle<float> bounds,
        float scaleFactor,
        juce::Rectangle<int> viewport);

class CurvePanelEnvironment {
public:
    TrimeshPanelEnvironment& services() { return environment; }

private:
    TrimeshPanelEnvironment environment;
};

class CurvePanelSnapshotCache {
public:
    void publish(juce::Image image, bool hasVisibleContent);
    bool paint(juce::Graphics& graphics, juce::Rectangle<float> bounds, bool resample) const;

private:
    mutable juce::CriticalSection lock;
    juce::Image image;
    bool visibleContent {};
};

class CurvePanelPreviewRenderCache {
public:
    struct Key {
        float width {};
        float height {};
        float scaleFactor {};
        uint64_t modelRevision {};
        uint64_t contentRevision {};
        uint64_t presentationRevision {};
        uint64_t invalidationGeneration {};

        bool operator==(const Key& other) const;
    };

    struct Diagnostics {
        uint64_t hits {};
        uint64_t misses {};
    };

    bool canReuse(const Key& key);
    void didRender(Key key);
    void invalidate();
    Diagnostics diagnostics() const;

private:
    Key renderedKey;
    bool valid {};
    std::atomic<uint64_t> hits {};
    std::atomic<uint64_t> misses {};
};

class CurvePanelInteractionAdapter {
public:
    virtual ~CurvePanelInteractionAdapter() = default;
    virtual void beginEdit() = 0;
    virtual void publishIntermediateRevision() = 0;
    virtual void commitEdit() = 0;
};

class CurvePanelHostDelegate : public CurvePanelInteractionAdapter {
public:
    virtual ~CurvePanelHostDelegate() = default;
    virtual void initialiseCurvePanel(Component* component) = 0;
    virtual void updateCurvePanelZoom(bool resetView) = 0;
    virtual void prepareCurvePanel() = 0;
    virtual void synchronizeCurvePanelSelection() = 0;
    virtual void repaintCurvePanel() = 0;
};

class CurvePanelHost : private RenderInvalidationTarget {
public:
    CurvePanelHost(Panel& panel, CurvePanelHostDelegate& delegate);
    ~CurvePanelHost();

    Component* component();
    Component* componentIfCreated();
    void render(Rectangle<float> bounds, Rectangle<float> clipBounds, float scaleFactor);
    void renderPreview(
            Rectangle<float> bounds,
            float scaleFactor,
            bool preserveInteractiveZoom,
            uint64_t modelRevision,
            uint64_t contentRevision,
            uint64_t presentationRevision);
    bool paintExpandedSnapshot(Graphics& graphics, Rectangle<float> bounds) const;
    bool paintPreviewSnapshot(Graphics& graphics, Rectangle<float> bounds) const;
    bool usesCursor(const MouseCursor& cursor) const;
    void releaseSharedGlResources();

    RenderInvalidationAccumulator::Diagnostics invalidationDiagnostics() const {
        return invalidation.diagnostics();
    }

    CurvePanelPreviewRenderCache::Diagnostics previewRenderDiagnostics() const {
        return previewRenderCache.diagnostics();
    }

private:
    class HostComponent;

    void initialiseComponent();
    void initialiseSharedGlResources();
    bool captureRenderedPanelImage(
            Rectangle<float> bounds,
            float scaleFactor,
            Image& destination,
            bool& hasVisibleContent) const;
    CurvePanelPreviewRenderCache::Key previewRenderKey(
            Rectangle<float> bounds,
            float scaleFactor,
            uint64_t modelRevision,
            uint64_t contentRevision,
            uint64_t presentationRevision) const;
    bool renderPreviewUncached(
            Rectangle<float> bounds,
            float scaleFactor,
            bool preserveInteractiveZoom);
    PanelHostCallbacks callbacks() const;
    void requestPanelInvalidation(PanelDirtyState::Flag flag);
    uint32_t availableRenderInvalidations() const override;
    void flushRenderInvalidations(uint32_t categories) override;

    Panel& panel;
    CurvePanelHostDelegate& delegate;
    std::unique_ptr<HostComponent> hostComponent;
    std::unique_ptr<GLPanelRenderer> panelRenderer;
    CommonGL* panelGfx {};
    CurvePanelSnapshotCache previewSnapshot;
    CurvePanelSnapshotCache expandedSnapshot;
    CurvePanelPreviewRenderCache previewRenderCache;
    RenderInvalidationAccumulator invalidation;
    std::atomic<uint64_t> previewInvalidationGeneration {};
    bool componentInitialised {};
    std::atomic<bool> sharedGlResourcesInitialised {};
    std::atomic<bool> renderSurfaceVisible {};
};

}

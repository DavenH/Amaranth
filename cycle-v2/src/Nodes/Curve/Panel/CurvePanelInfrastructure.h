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
class ZoomPanel;

namespace CycleV2 {

void stylePanelZoomScrollbars(ZoomPanel& panel);

std::optional<juce::Rectangle<int>> curvePanelFramebufferReadBounds(
        juce::Rectangle<float> bounds,
        float scaleFactor,
        juce::Rectangle<int> viewport);

juce::Image curvePanelSnapshotFromPixels(
        const juce::PixelARGB* pixels, int width, int height, bool& hasVisibleContent);

class CurvePanelEnvironment {
public:
    TrimeshPanelEnvironment& services() { return environment; }

private:
    TrimeshPanelEnvironment environment;
};

class CurvePanelSnapshotCache {
public:
    bool publish(juce::Image image, bool hasVisibleContent);
    bool paint(juce::Graphics& graphics, juce::Rectangle<float> bounds, bool resample) const;
    bool hasVisibleSnapshot() const;
    void clear();
    uint64_t revision() const { return publicationRevision.load(); }

private:
    bool visibleContent {};
    std::atomic<uint64_t> publicationRevision {};

    mutable juce::CriticalSection lock;
    juce::Image image;
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
        juce::Rectangle<float> view;

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
    std::atomic<bool> valid {};
    std::atomic<uint64_t> hits {};
    std::atomic<uint64_t> misses {};
};

class CurvePanelInteractionAdapter {
public:
    virtual ~CurvePanelInteractionAdapter() = default;
    virtual void beginEdit() = 0;
    virtual void publishIntermediateRevision() = 0;
    virtual void commitEdit() = 0;
    virtual bool cancelEdit() { return false; }
};

class CurvePanelHostDelegate : public CurvePanelInteractionAdapter {
public:
    virtual ~CurvePanelHostDelegate() = default;
    virtual bool allowsCurvePanelEditing() const { return true; }
    virtual bool deleteCurvePanelSelection() { return false; }
    virtual void initialiseCurvePanel(Component* component) = 0;
    virtual void updateCurvePanelZoom(bool resetView) = 0;
    virtual void prepareCurvePanel() = 0;
    virtual void synchronizeCurvePanelSelection() = 0;
    virtual void repaintCurvePanel() = 0;
    virtual void setCurvePanelCursor(const MouseCursor& cursor) = 0;
};

class CurvePanelHost : private RenderInvalidationTarget {
public:
    CurvePanelHost(Panel& panel, CurvePanelHostDelegate& delegate);
    ~CurvePanelHost();

    Component* component();
    Component* componentIfCreated();
    Component* zoomComponent();
    Component* zoomComponentIfCreated();
    Rectangle<float> contentBounds(Rectangle<float> frame) const;
    var zoomControlsState() const;
    void render(Rectangle<float> bounds, Rectangle<float> clipBounds, float scaleFactor,
            uint64_t modelRevision = 0, uint64_t contentRevision = 0, uint64_t presentationRevision = 0);
    bool renderPreview(
            Rectangle<float> bounds,
            float scaleFactor,
            bool preserveInteractiveZoom,
            uint64_t modelRevision,
            uint64_t contentRevision,
            uint64_t presentationRevision);
    bool paintExpandedSnapshot(Graphics& graphics, Rectangle<float> bounds) const;
    bool paintPreviewSnapshot(Graphics& graphics, Rectangle<float> bounds) const;
    bool hasVisiblePreviewSnapshot() const { return previewSnapshot.hasVisibleSnapshot(); }
    uint64_t previewSnapshotRevision() const { return previewSnapshot.revision(); }
    uint64_t layoutCount() const { return renderLayoutCount.load(); }
    void appendRenderDiagnostics(DynamicObject& state) const;
    bool usesCursor(const MouseCursor& cursor) const;
    void resetDocumentPresentation();
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
    void updateRenderLayout(Rectangle<float> bounds, float scaleFactor);
    bool hasPendingRenderChanges() const;
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
    CurvePanelPreviewRenderCache expandedRenderCache;
    RenderInvalidationAccumulator invalidation;
    std::atomic<uint64_t> previewInvalidationGeneration {};
    std::atomic<uint64_t> expandedInvalidationGeneration {};
    std::atomic<uint64_t> renderLayoutCount {};
    mutable std::atomic<uint64_t> snapshotReadbackCount {};
    mutable std::atomic<uint64_t> snapshotReadbackMicroseconds {};
    mutable std::atomic<uint64_t> snapshotConversionMicroseconds {};
    Rectangle<float> lastRenderBounds;
    Rectangle<float> lastRenderZoom;
    float lastRenderScaleFactor {};
    bool componentInitialised {};
    std::atomic<bool> sharedGlResourcesInitialised {};
    std::atomic<bool> renderSurfaceVisible {};
};

}

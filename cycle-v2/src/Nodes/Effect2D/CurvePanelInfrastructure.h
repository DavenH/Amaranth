#pragma once

#include <JuceHeader.h>
#include <UI/Panels/PanelHostContext.h>

#include <atomic>
#include <functional>
#include <memory>

#include "../../UI/RenderInvalidationAccumulator.h"
#include "../Trimesh/TrimeshPanelEnvironment.h"

class CommonGL;
class GLPanelRenderer;
class Panel;

namespace CycleV2 {

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
    virtual void setCurvePanelCursor(const MouseCursor& cursor) = 0;
};

class CurvePanelHost : private RenderInvalidationTarget {
public:
    CurvePanelHost(Panel& panel, CurvePanelHostDelegate& delegate);
    ~CurvePanelHost();

    Component* component();
    Component* componentIfCreated();
    void render(Rectangle<float> bounds, Rectangle<float> clipBounds, float scaleFactor);
    void renderPreview(Rectangle<float> bounds, float scaleFactor);
    bool paintExpandedSnapshot(Graphics& graphics, Rectangle<float> bounds) const;
    bool paintPreviewSnapshot(Graphics& graphics, Rectangle<float> bounds) const;
    bool usesCursor(const MouseCursor& cursor) const;
    void releaseSharedGlResources();

    RenderInvalidationAccumulator::Diagnostics invalidationDiagnostics() const {
        return invalidation.diagnostics();
    }

private:
    class HostComponent;

    void initialiseComponent();
    void initialiseSharedGlResources();
    void captureRenderedPanelImage(
            Rectangle<float> bounds,
            float scaleFactor,
            Image& destination,
            bool& hasVisibleContent) const;
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
    RenderInvalidationAccumulator invalidation;
    bool componentInitialised {};
    std::atomic<bool> sharedGlResourcesInitialised {};
    std::atomic<bool> renderSurfaceVisible {};
};

}

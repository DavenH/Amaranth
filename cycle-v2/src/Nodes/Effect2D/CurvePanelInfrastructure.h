#pragma once

#include "../Trimesh/TrimeshPanelEnvironment.h"

#include <JuceHeader.h>
#include <UI/Panels/PanelHostContext.h>

#include <functional>
#include <memory>

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

class CurvePanelHost {
public:
    CurvePanelHost(
            Panel& panel,
            std::function<void(Component*)> initialisePanel,
            std::function<void(bool)> updateZoom,
            std::function<void()> preparePanel,
            std::function<bool()> publishEdit,
            std::function<void()> synchronizeSelection);
    ~CurvePanelHost();

    Component* component();
    Component* componentIfCreated();
    void setCallbacks(
            std::function<void()> repaintCallback,
            std::function<void(const MouseCursor&)> cursorCallback);
    void render(Rectangle<float> bounds, Rectangle<float> clipBounds, float scaleFactor);
    void renderPreview(Rectangle<float> bounds, float scaleFactor);
    bool paintExpandedSnapshot(Graphics& graphics, Rectangle<float> bounds) const;
    bool paintPreviewSnapshot(Graphics& graphics, Rectangle<float> bounds) const;
    void releaseSharedGlResources();

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

    Panel& panel;
    std::function<void(Component*)> initialisePanel;
    std::function<void(bool)> updateZoom;
    std::function<void()> preparePanel;
    std::function<bool()> publishEdit;
    std::function<void()> synchronizeSelection;
    std::function<void()> repaintCallback;
    std::function<void(const MouseCursor&)> cursorCallback;
    std::unique_ptr<HostComponent> hostComponent;
    std::unique_ptr<GLPanelRenderer> panelRenderer;
    CommonGL* panelGfx {};
    CurvePanelSnapshotCache previewSnapshot;
    CurvePanelSnapshotCache expandedSnapshot;
    bool componentInitialised {};
    bool sharedGlResourcesInitialised {};
};

}

#pragma once

#include <memory>

#include "JuceHeader.h"
#include "OpenGLBase.h"
#include "PanelCompositor.h"

class Panel;

class SharedPanelCanvas :
        public juce::Component
    ,   private juce::Timer
    ,   private juce::OpenGLRenderer
    ,   public OpenGLBase {
public:
    SharedPanelCanvas();
    ~SharedPanelCanvas() override;

    PanelCompositor& getCompositor() { return compositor; }
    const PanelCompositor& getCompositor() const { return compositor; }

    void attachOpenGL();
    void attachOpenGL(juce::Component& target, bool enableComponentPainting);
    void detachOpenGL();
    bool isOpenGLAttached() const { return context.isAttached() || openGLAttachRequested; }
    void invalidateAll(PanelDirtyState::Flag flag = PanelDirtyState::Flag::Full);
    void invalidatePanel(Panel* panel, PanelDirtyState::Flag flag);
    void registerOrUpdatePanel(Panel* panel, const juce::Rectangle<int>& bounds, bool visible);
    void removePanel(Panel* panel);
    void setDebugSnapshotOverlayEnabled(bool enabled);
    bool isDebugSnapshotOverlayEnabled() const { return debugSnapshotOverlayEnabled; }

    void paint(juce::Graphics& g) override;

private:
    struct RenderState;

    RenderState* getOrCreateRenderState(Panel* panel);
    void clearCurrentPanelViewport();
    void drawDebugOutlinesOpenGL();
    void initialiseRenderState(RenderState& state);
    void renderSharedPanel(RenderState& state, const PanelCompositor::Entry& entry);
    void resetSharedViewport();
    void setSharedViewport(const juce::Rectangle<int>& bounds);
    void repaintDirtyRegions();
    void repaintCachedSurfaces();
    void newOpenGLContextCreated() override;
    void openGLContextClosing() override;
    void renderOpenGL() override;
    void timerCallback() override;

    PanelCompositor compositor;
    std::vector<std::unique_ptr<RenderState>> renderStates;
    juce::Rectangle<int> currentPanelBounds;
    double currentRenderScale = 1.0;
    bool debugSnapshotOverlayEnabled = false;
    bool openGLAttachRequested = false;
    bool openGLReady = false;
};

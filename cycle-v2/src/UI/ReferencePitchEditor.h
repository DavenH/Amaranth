#pragma once

#include <JuceHeader.h>
#include <Curve/Mesh/EnvelopeMesh.h>

#include <functional>
#include <memory>

#include "Nodes/Curve/Panel/CurvePanelInfrastructure.h"
#include "Reference/ReferenceLibrary.h"
#include "UI/ExpandedEditorChrome.h"
#include "UI/PanelOverlayIconButton.h"
#include "UI/ReferenceSurfaceCache.h"

namespace CycleV2 {

class EnvelopeCurvePanelContract;
class ComparisonSurfaceView;

class ReferencePitchEditor final :
        public juce::Component
    ,   private CurvePanelHostDelegate
    ,   private juce::ScrollBar::Listener {
public:
    explicit ReferencePitchEditor(ReferenceLibrary& library);
    ~ReferencePitchEditor() override;

    bool open(const juce::String& referenceId);
    void syncFromLibrary();
    void refreshCyclogram();
    const juce::String& referenceId() const { return openedId; }
    juce::Rectangle<int> cyclogramBounds() const;
    juce::Rectangle<int> closeBounds() const;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> pointerTargetsForAutomation() const;
    void renderOpenGL(float scaleFactor);
    void releaseOpenGLResources();
    juce::var exportAutomationState() const;
    juce::Rectangle<int> curveBounds() const;

    void paint(juce::Graphics& graphics) override;
    void paintOverChildren(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    std::function<void()> onEdited;
    std::function<void()> onClose;
    std::function<void()> onAuditionToggle;
    std::function<void()> onRenderRequested;
    std::function<void()> onViewModeChanged;

private:
    void configureViewControls();
    void setSurfaceMode(bool spectral);
    void syncTimeZoom(bool fromSurface);
    void resetView();
    void scrollBarMoved(juce::ScrollBar* scrollbar, double newRangeStart) override;
    void initialiseCurvePanel(juce::Component* component) override;
    void updateCurvePanelZoom(bool resetView) override;
    void prepareCurvePanel() override;
    void synchronizeCurvePanelSelection() override;
    void repaintCurvePanel() override;
    void setCurvePanelCursor(const juce::MouseCursor& cursor) override;
    void beginEdit() override;
    void publishIntermediateRevision() override;
    void commitEdit() override;
    bool cancelEdit() override;

    ReferenceLibrary& library;
    ExpandedEditorChrome chrome;
    std::unique_ptr<ComparisonSurfaceView> cyclogram;
    ReferenceSurfaceCache surfaceCache;
    PanelOverlayIconButton timeView;
    PanelOverlayIconButton spectrumView;
    PanelOverlayIconButton zoomAttack;
    PanelOverlayIconButton zoomFit;
    PanelOverlayIconButton zoomFull;
    int displayedGeneration { -1 };
    bool spectral {};
    bool displayedSpectral {};
    bool linkingTimeZoom {};
    CurvePanelEnvironment environment;
    EnvelopeMesh mesh { "ReferencePitchEnvelope" };
    std::unique_ptr<EnvelopeCurvePanelContract> panel;
    std::unique_ptr<CurvePanelHost> host;
    juce::String openedId;
    bool editingMesh {};
};

}

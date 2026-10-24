#pragma once

#include <JuceHeader.h>
#include <Curve/Mesh/EnvelopeMesh.h>

#include <functional>
#include <memory>

#include "Nodes/Curve/Panel/CurvePanelInfrastructure.h"
#include "Reference/ReferenceLibrary.h"
#include "UI/Editors/PropertyControls.h"
#include "UI/ExpandedEditorChrome.h"

namespace CycleV2 {

class EnvelopeCurvePanelContract;
class ComparisonSurfaceView;

class ReferencePitchEditor final :
        public juce::Component
    ,   private CurvePanelHostDelegate {
public:
    explicit ReferencePitchEditor(ReferenceLibrary& library);
    ~ReferencePitchEditor() override;

    bool open(const juce::String& referenceId);
    void syncFromLibrary();
    void refreshCyclogram();
    const juce::String& referenceId() const { return openedId; }
    juce::Rectangle<int> cyclogramBounds() const;
    juce::Rectangle<int> closeBounds() const;
    void renderOpenGL(float scaleFactor);
    void releaseOpenGLResources();
    juce::var exportAutomationState() const;
    juce::Rectangle<int> curveBounds() const;

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    std::function<void()> onEdited;
    std::function<void()> onClose;
    std::function<void()> onRenderRequested;

private:
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
    void commitFundamental();

    ReferenceLibrary& library;
    ExpandedEditorChrome chrome;
    std::unique_ptr<ComparisonSurfaceView> cyclogram;
    const PitchedSample* displayedSample {};
    uint64_t displayedRevision {};
    CurvePanelEnvironment environment;
    EnvelopeMesh mesh { "ReferencePitchEnvelope" };
    std::unique_ptr<EnvelopeCurvePanelContract> panel;
    std::unique_ptr<CurvePanelHost> host;
    PropertySliderRow fundamental;
    juce::String openedId;
    bool editingMesh {};
    bool draggingFundamental {};
};

}

#pragma once

#include "../../Graph/NodeGraph.h"
#include "Effect2DWidget.h"

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class Effect2DExpandedEditorComponent : public Component {
public:
    struct Callbacks {
        std::function<void()> close;
        std::function<void()> repaintOpenGL;
        std::function<void(const String& parameterId, const String& label, const String& value)> setNodeParameter;
    };

    explicit Effect2DExpandedEditorComponent(Effect2DWidget& widget);
    ~Effect2DExpandedEditorComponent() override;

    void setCallbacks(Callbacks nextCallbacks);
    void setNode(const Node& nextNode);
    void renderOpenGL(float scaleFactor);
    Rectangle<float> panelBoundsForAutomation() const;
    var automationState() const;

    void paint(Graphics& g) override;
    void resized() override;
    void mouseMove(const MouseEvent& event) override;
    void mouseDown(const MouseEvent& event) override;
    void mouseDrag(const MouseEvent& event) override;
    void mouseUp(const MouseEvent& event) override;

private:
    struct ControlSet {
        ToggleButton enabled;
        Slider firstSlider;
        Slider secondSlider;
        Slider thirdSlider;
        ComboBox menu;
        TextButton firstButton;
        TextButton secondButton;
        TextButton thirdButton;
        Label firstLabel;
        Label secondLabel;
        Label thirdLabel;
        Label menuLabel;
    };

    Rectangle<float> closeButtonBounds() const;
    Rectangle<float> contentBounds() const;
    Rectangle<float> panelBounds() const;
    Rectangle<int> controlBounds() const;
    void configureControls();
    void styleSlider(Slider& slider, Label& label, const String& text);
    void styleButton(Button& button, const String& text);
    void styleMenu(const StringArray& items, const String& selected, const String& labelText);
    void updateEnvelopeMarkerButtons();
    void syncEnvelopeStateFromNode();
    void syncEnvelopeAxisLinksToWidget();
    void syncControlValuesFromNode();
    void updatePanelHost();
    void updateControlLayout();
    bool updateEnvelopeMorphFromPoint(juce::Point<float> position);
    bool updateEnvelopeViewAxisFromPoint(juce::Point<float> position);
    bool beginVertexParameterEdit(juce::Point<float> position);
    bool updateVertexParameterEdit(juce::Point<float> position);
    bool showVertexParameterGuideMenu(juce::Point<float> position);
    bool findVertexParameterAt(juce::Point<float> position, String& parameterId, float& value) const;
    bool findVertexParameterValueAt(
            const String& parameterId,
            juce::Point<float> position,
            float& value) const;
    bool findVertexGuideAt(juce::Point<float> position, String& parameterId, Rectangle<int>& targetBounds) const;
    void pushControlValues();

    Effect2DWidget& widget;
    Callbacks callbacks;
    Node node;
    ControlSet controls;
    NodeKind configuredKind { NodeKind::GenericProcessor };
    int envelopeViewAxis {};
    bool envelopeAxisLinked[3] { true, true, true };
    bool envelopeLogarithmic {};
    bool draggingEnvelopeMorphPlane {};
    bool draggingVertexParameter {};
    String activeVertexParameterId;
    bool syncingControls {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Effect2DExpandedEditorComponent)
};

}

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
    };

    explicit Effect2DExpandedEditorComponent(Effect2DWidget& widget);
    ~Effect2DExpandedEditorComponent() override;

    void setCallbacks(Callbacks nextCallbacks);
    void setNode(const Node& nextNode);
    void renderOpenGL(float scaleFactor);

    void paint(Graphics& g) override;
    void resized() override;
    void mouseMove(const MouseEvent& event) override;
    void mouseDown(const MouseEvent& event) override;

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
    String subtitleForNode() const;
    void configureControls();
    void styleSlider(Slider& slider, Label& label, const String& text);
    void styleButton(Button& button, const String& text);
    void styleMenu(const StringArray& items, const String& selected, const String& labelText);
    void updatePanelHost();
    void updateControlLayout();

    Effect2DWidget& widget;
    Callbacks callbacks;
    Node node;
    ControlSet controls;
    NodeKind configuredKind { NodeKind::GenericProcessor };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Effect2DExpandedEditorComponent)
};

}

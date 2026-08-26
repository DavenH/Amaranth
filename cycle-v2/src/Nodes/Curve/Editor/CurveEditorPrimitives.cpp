#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"

#include "Nodes/Curve/Model/CurveNodeModels.h"

namespace CycleV2 {

LabeledParameterSlider::LabeledParameterSlider(Component& owner, const String& labelText) :
        PropertySliderRow(owner, labelText) {}

void LabeledParameterSlider::setBounds(Rectangle<int> bounds, int labelWidth, int gap) {
    PropertySliderRow::setBounds(bounds, labelWidth, gap);
}

ParameterToggle::ParameterToggle(Component& owner, const String& labelText) {
    stylePropertyLabel(label, labelText);
    button.setButtonText({});
    owner.addAndMakeVisible(label);
    owner.addAndMakeVisible(button);
}

void ParameterToggle::setBounds(Rectangle<int> bounds, int labelWidth, int gap) {
    label.setBounds(bounds.removeFromLeft(labelWidth));
    bounds.removeFromLeft(gap);
    button.setBounds(bounds);
}

void ParameterRail::layout(
        Rectangle<float> area,
        ParameterToggle& enabled,
        std::initializer_list<LabeledParameterSlider*> sliders,
        std::initializer_list<TextButton*> buttons) {
    Rectangle<int> bounds = area.toNearestInt().reduced(12, 8);
    enabled.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(12);

    for (auto* slider : sliders) {
        slider->setBounds(bounds.removeFromTop(32));
        bounds.removeFromTop(12);
    }

    for (auto* button : buttons) {
        auto row = bounds.removeFromTop(32);
        row.removeFromLeft(94);
        button->setBounds(row.removeFromLeft(76).reduced(0, 2));
        bounds.removeFromTop(7);
    }
}

void addEditorParameter(
        std::vector<NodeParameter>& result,
        const Node& node,
        const String& id,
        const String& name,
        const String& value) {
    String resolvedName = name;
    for (const auto& parameter : node.parameters) {
        if (parameter.id == id && parameter.label.isNotEmpty()) {
            resolvedName = parameter.label;
            break;
        }
    }
    result.push_back({ id, resolvedName, value });
}

String retainedEditorParameter(const Node& node, const String& id, const String& fallback) {
    return parameterValueForNode(node, id, fallback);
}

var editorBoundsToVar(Rectangle<float> bounds) {
    auto* result = new DynamicObject();
    result->setProperty("x", bounds.getX());
    result->setProperty("y", bounds.getY());
    result->setProperty("width", bounds.getWidth());
    result->setProperty("height", bounds.getHeight());
    return result;
}

}

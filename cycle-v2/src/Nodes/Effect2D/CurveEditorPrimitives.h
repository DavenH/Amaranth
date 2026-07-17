#pragma once

#include "../../Graph/NodeGraph.h"

#include <JuceHeader.h>

#include <functional>
#include <initializer_list>
#include <utility>
#include <vector>

namespace CycleV2 {

class LabeledParameterSlider {
public:
    LabeledParameterSlider(Component& owner, const String& labelText);
    ~LabeledParameterSlider();

    void bind(const std::function<void()>& publish,
            const std::function<void()>& beginTransaction,
            const std::function<void()>& commitTransaction);
    void setBounds(Rectangle<int> bounds, int labelWidth = 82, int gap = 12);

    Label label;
    Slider slider;
};

class ParameterToggle {
public:
    ParameterToggle(Component& owner, const String& labelText);

    void bind(const std::function<void()>& publish);
    void setBounds(Rectangle<int> bounds, int labelWidth = 82, int gap = 12);

    Label label;
    ToggleButton button;
};

class ParameterRail {
public:
    static void layout(
            Rectangle<float> area,
            ParameterToggle& enabled,
            std::initializer_list<LabeledParameterSlider*> sliders,
            std::initializer_list<TextButton*> buttons = {});
};

void styleParameterLabel(Label& label, const String& text);
void styleParameterButton(TextButton& button, const String& text);
void addEditorParameter(
        std::vector<NodeParameter>& result,
        const Node& node,
        const String& id,
        const String& name,
        const String& value);
String retainedEditorParameter(const Node& node, const String& id, const String& fallback);
var editorBoundsToVar(Rectangle<float> bounds);

}

#include "UI/ModulationNodeEditors.h"

#include "Graph/NodeParameterMap.h"
#include "UI/ExpandedEditorChrome.h"
#include "UI/Editors/NodePropertyControlBinding.h"

namespace CycleV2 {

namespace {

String parameterId(const String& prefix, const String& name) {
    if (prefix.isEmpty()) {
        return name;
    }
    return prefix + name.substring(0, 1).toUpperCase() + name.substring(1);
}

int sourceRowHeight(const String& source) {
    return source == "constant" || source == "midiCC" ? 88 : 44;
}

class ModulationSourceRow final : public Component {
public:
    ModulationSourceRow(
            String prefixToUse,
            String axisLabelToUse,
            String defaultSourceToUse,
            NodeEditorCommands& commandsToUse) :
            prefix        (std::move(prefixToUse))
        ,   axisLabel     (std::move(axisLabelToUse))
        ,   defaultSource (std::move(defaultSourceToUse))
        ,   commands      (commandsToUse)
        ,   controller    (*this, commands, parameterId(prefix, "controller"), "CC")
        ,   constant      (*this, commands, parameterId(prefix, "constant"), "Value") {
        StringArray names {
                "Voice Time", "Velocity", "1-Velocity", "Key Scale",
                "Mod Wheel", "Channel Pressure", "MIDI CC", "Constant"
        };
        sourceIds = {
                "voiceTime", "velocity", "inverseVelocity", "keyScale",
                "modWheel", "channelPressure", "midiCC", "constant"
        };
        if (prefix.isNotEmpty()) {
            names.insert(0, "Unmapped");
            sourceIds.insert(0, "unmapped");
        }
        for (int index = 0; index < names.size(); ++index) {
            source.addItem(names[index], index + 1);
        }
        source.onChange = [this] {
            if (!binding && source.getSelectedItemIndex() >= 0) {
                commands.setNodeParameterText(
                        nodeId,
                        parameterId(prefix, "source"),
                        parameterLabel("Source"),
                        selectedSourceId());
                updateConditionalControl();
            }
        };
        source.setComponentID("modulation." + prefix + ".source");
        controller.slider.setComponentID("modulation." + prefix + ".controller");
        constant.slider.setComponentID("modulation." + prefix + ".constant");
        addAndMakeVisible(source);

        source.setTooltip(prefix.isEmpty() ? "Choose the modulation signal to generate."
                : "Default morph mapping. Unmapped uses the destination's authored position. Explicit cables override defaults.");
        controller.setCompactLayout(true);
        constant.setCompactLayout(true);
        controller.configureValuePresentation(
                [](double value) { return String(roundToInt(value)); },
                [](const String& text) { return parsePropertyNumber(text); },
                1.0, 1.0, 1.0, "MIDI controller number, 0 to 127");
        constant.configureValuePresentation(
                [](double value) { return formatPropertyReal(value); },
                [](const String& text) { return parsePropertyNumber(text); },
                0.5, 0.01, 0.001, "Constant value. Shift-drag for fine adjustment; enter an exact value.");
        controller.slider.setRange(0.0, 127.0, 1.0);
        constant.slider.setRange(0.0, 1.0, 0.001);
    }

    void setNode(const Node& node) {
        nodeId = node.id;
        binding = true;
        const NodeParameterMap parameters(node);
        const String sourceId = parameters.stringValue(
                parameterId(prefix, "source"), defaultSource);
        source.setSelectedItemIndex(
                jmax(0, sourceIds.indexOf(sourceId)),
                dontSendNotification);
        controller.bind(nodeId, parameters.floatValue(parameterId(prefix, "controller"), 1.f));
        constant.bind(nodeId, parameters.floatValue(parameterId(prefix, "constant"), 0.5f));
        binding = false;
        updateConditionalControl();
    }

    int preferredHeight() const { return sourceRowHeight(selectedSourceId()); }

    void paint(Graphics& graphics) override {
        if (axisLabel.isEmpty()) {
            return;
        }
        graphics.setColour(axisColour());
        graphics.setFont(FontOptions(14.f));
        graphics.drawText(axisLabel, 0, 0, 26, 30, Justification::centred);
    }

    void resized() override {
        auto menu = getLocalBounds().removeFromTop(32);
        if (axisLabel.isNotEmpty()) {
            menu.removeFromLeft(30);
        }
        source.setBounds(menu.reduced(2, 2));
        const auto values = getLocalBounds().withTrimmedTop(38).withHeight(PropertyControlMetrics::compactRowHeight);
        controller.setBounds(values);
        constant.setBounds(values);
    }

    var automationState() const {
        auto* state = new DynamicObject();
        state->setProperty("source", selectedSourceId());
        state->setProperty("controller", controller.slider.getValue());
        state->setProperty("constant", constant.slider.getValue());
        state->setProperty("constantControl", propertySliderRowAutomationState(constant));
        state->setProperty("controllerControl", propertySliderRowAutomationState(controller));
        state->setProperty("constantVisible", constant.slider.isVisible());
        state->setProperty("controllerVisible", controller.slider.isVisible());
        auto* bounds = new DynamicObject();
        bounds->setProperty("x", getX());
        bounds->setProperty("y", getY());
        bounds->setProperty("width", getWidth());
        bounds->setProperty("height", getHeight());
        state->setProperty("rowBounds", var(bounds));
        return state;
    }

private:
    String parameterLabel(const String& name) const {
        return axisLabel.isEmpty() ? name : axisLabel + " " + name;
    }

    String selectedSourceId() const {
        const int index = source.getSelectedItemIndex();
        return isPositiveAndBelow(index, sourceIds.size())
                ? sourceIds[index]
                : defaultSource;
    }

    Colour axisColour() const {
        if (prefix.endsWithIgnoreCase("yellow")) {
            return colourForMorphDimension(MorphDimension::Yellow);
        }
        if (prefix.endsWithIgnoreCase("red")) {
            return colourForMorphDimension(MorphDimension::Red);
        }
        return colourForMorphDimension(MorphDimension::Blue);
    }

    void updateConditionalControl() {
        const String selected = selectedSourceId();
        const auto visibility = [](PropertySliderRow& row, bool visible) {
            row.label.setVisible(visible);
            row.slider.setVisible(visible);
            row.value.setVisible(visible);
            row.unit.setVisible(false);
        };
        visibility(controller, selected == "midiCC");
        visibility(constant, selected == "constant");
        resized();
    }

    String prefix;
    String axisLabel;
    String defaultSource;
    NodeEditorCommands& commands;
    String nodeId;
    bool binding {};
    StringArray sourceIds;
    ComboBox source;
    NodePropertySliderRow controller;
    NodePropertySliderRow constant;
};

class ModulationEditorComponent final : public Component {
public:
    ModulationEditorComponent(
            NodeKind kindToUse,
            NodeEditorCommands& commands,
            NodeEditorPresentation& presentationToUse) :
            kind         (kindToUse)
        ,   presentation (presentationToUse)
        ,   chrome       (*this,
                    kind == NodeKind::ModulationTriple
                            ? "MODULATION TRIPLE"
                            : "MODULATION",
                    "modulationEditor",
                    [this] { presentation.closeNodeEditor(); }) {

        if (kind == NodeKind::ModulationTriple) {
            addRow("yellow", "Y", "voiceTime", commands);
            addRow("red", "R", "keyScale", commands);
            addRow("blue", "B", "modWheel", commands);
            addRow("envelopeYellow", "Y", "unmapped", commands);
            addRow("envelopeRed", "R", "keyScale", commands);
            addRow("envelopeBlue", "B", "modWheel", commands);
        } else {
            addRow({}, {}, "modWheel", commands);
        }
    }

    void setNode(const Node& node) {
        for (auto& row : rows) {
            row->setNode(node);
        }
    }

    void paint(Graphics& graphics) override {
        chrome.paint(graphics);
        if (kind == NodeKind::ModulationTriple) {
            auto headings = getLocalBounds().reduced(16).withTrimmedTop(34).removeFromTop(24);
            graphics.setColour(Colour(0xffb8c1cd));
            graphics.setFont(FontOptions(12.f));
            graphics.drawText("Trimesh", headings.removeFromLeft(headings.getWidth() / 2),
                    Justification::centred);
            graphics.drawText("Envelope", headings, Justification::centred);
        }
    }

    void resized() override {
        chrome.resized();
        Rectangle<int> area = getLocalBounds().reduced(16);
        area.removeFromTop(34);
        if (kind == NodeKind::ModulationTriple) {
            area.removeFromTop(24);
            auto left = area.removeFromLeft(area.getWidth() / 2);
            left.removeFromRight(8);
            area.removeFromLeft(8);
            for (size_t axis = 0; axis < 3; ++axis) {
                const int rowHeight = jmax(rows[axis]->preferredHeight(), rows[axis + 3]->preferredHeight());
                rows[axis]->setBounds(left.removeFromTop(rowHeight));
                rows[axis + 3]->setBounds(area.removeFromTop(rowHeight));
            }
        } else {
            rows.front()->setBounds(area.removeFromTop(jmax(48, area.getHeight())));
        }
    }

    var automationState() const {
        if (kind != NodeKind::ModulationTriple) {
            return rows.front()->automationState();
        }

        auto* state = new DynamicObject();
        state->setProperty("yellow", rows[0]->automationState());
        state->setProperty("red", rows[1]->automationState());
        state->setProperty("blue", rows[2]->automationState());
        auto* envelope = new DynamicObject();
        envelope->setProperty("yellow", rows[3]->automationState());
        envelope->setProperty("red", rows[4]->automationState());
        envelope->setProperty("blue", rows[5]->automationState());
        state->setProperty("envelope", var(envelope));
        return state;
    }

private:
    void addRow(
            String prefix,
            String axisLabel,
            String defaultSource,
            NodeEditorCommands& commands) {
        auto row = std::make_unique<ModulationSourceRow>(
                std::move(prefix),
                std::move(axisLabel),
                std::move(defaultSource),
                commands);
        addAndMakeVisible(*row);
        rows.push_back(std::move(row));
    }

    NodeKind kind;
    NodeEditorPresentation& presentation;
    ExpandedEditorChrome chrome;
    std::vector<std::unique_ptr<ModulationSourceRow>> rows;
};

class ModulationNodeEditor final : public NodeEditor {
public:
    ModulationNodeEditor(const Node& node, const NodeEditorContext& context) :
            kind   (node.kind)
        ,   editor (node.kind, context.commands, context.presentation) {}

    Component& component() override { return editor; }
    void bind(const Node& node) override { editor.setNode(node); }
    void renderOpenGL(float) override {}
    void appendAutomationState(DynamicObject& state) const override {
        state.setProperty(
                kind == NodeKind::ModulationTriple
                        ? "modulationTriple"
                        : "modulationSource",
                editor.automationState());
    }
    Rectangle<float> panelBoundsForAutomation() const override {
        return editor.getLocalBounds().toFloat();
    }
    void releaseOpenGLResources() override {}

private:
    NodeKind kind;
    ModulationEditorComponent editor;
};

class ModulationNodeEditorFactory final : public NodeEditorFactory {
public:
    Rectangle<float> layoutBounds(const Node& node, Rectangle<float> bounds) const override {
        const NodeParameterMap parameters(node);
        int height = 66;
        if (node.kind == NodeKind::ModulationTriple) {
            height += 24;
            for (const String axis : { "Yellow", "Red", "Blue" }) {
                height += jmax(sourceRowHeight(parameters.stringValue(axis.toLowerCase() + "Source")),
                        sourceRowHeight(parameters.stringValue("envelope" + axis + "Source")));
            }
        } else {
            height += sourceRowHeight(parameters.stringValue("source"));
        }
        return bounds.withHeight(jmin(bounds.getHeight(), (float) height));
    }

    std::unique_ptr<NodeEditor> create(
            const Node& node,
            const NodeEditorContext& context) const override {
        return std::make_unique<ModulationNodeEditor>(node, context);
    }
};

}

std::unique_ptr<NodeEditorFactory> createModulationNodeEditorFactory() {
    return std::make_unique<ModulationNodeEditorFactory>();
}

}

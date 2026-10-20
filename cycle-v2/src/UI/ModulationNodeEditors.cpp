#include "UI/ModulationNodeEditors.h"

#include "Graph/NodeParameterMap.h"
#include "UI/ExpandedEditorChrome.h"

namespace CycleV2 {

namespace {

constexpr int kSourceMenuWidth = 166;

String parameterId(const String& prefix, const String& name) {
    if (prefix.isEmpty()) {
        return name;
    }
    return prefix + name.substring(0, 1).toUpperCase() + name.substring(1);
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
        ,   commands      (commandsToUse) {
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
        addAndMakeVisible(source);

        controller.setSliderStyle(Slider::LinearHorizontal);
        controller.setTextBoxStyle(Slider::TextBoxRight, false, 54, 22);
        controller.setRange(0.0, 127.0, 1.0);
        controller.onValueChange = [this] {
            if (!binding) {
                commands.setNodeParameterValue(
                        nodeId,
                        parameterId(prefix, "controller"),
                        parameterLabel("Controller"),
                        (float) controller.getValue());
            }
        };
        addAndMakeVisible(controller);

        constant.setSliderStyle(Slider::LinearHorizontal);
        constant.setTextBoxStyle(Slider::TextBoxRight, false, 54, 22);
        constant.setRange(0.0, 1.0, 0.001);
        constant.onValueChange = [this] {
            if (!binding) {
                commands.setNodeParameterValue(
                        nodeId,
                        parameterId(prefix, "constant"),
                        parameterLabel("Constant"),
                        (float) constant.getValue());
            }
        };
        addAndMakeVisible(constant);
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
        controller.setValue(
                parameters.floatValue(parameterId(prefix, "controller"), 1.f),
                dontSendNotification);
        constant.setValue(
                parameters.floatValue(parameterId(prefix, "constant"), 0.5f),
                dontSendNotification);
        binding = false;
        updateConditionalControl();
    }

    void paint(Graphics& graphics) override {
        if (axisLabel.isEmpty()) {
            return;
        }
        graphics.setColour(axisColour());
        graphics.setFont(FontOptions(14.f));
        graphics.drawText(axisLabel, 0, 0, 26, getHeight(), Justification::centred);
    }

    void resized() override {
        Rectangle<int> area = getLocalBounds();
        if (axisLabel.isNotEmpty()) {
            area.removeFromLeft(30);
        }
        source.setBounds(area.removeFromLeft(jmin(kSourceMenuWidth, area.getWidth())).reduced(2, 8));
        controller.setBounds(area.reduced(4, 8));
        constant.setBounds(area.reduced(4, 8));
    }

    var automationState() const {
        auto* state = new DynamicObject();
        state->setProperty("source", selectedSourceId());
        state->setProperty("controller", controller.getValue());
        state->setProperty("constant", constant.getValue());
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
        controller.setVisible(selected == "midiCC");
        constant.setVisible(selected == "constant");
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
    Slider controller;
    Slider constant;
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
            const int rowHeight = jmax(48, area.getHeight() / 3);
            for (size_t axis = 0; axis < 3; ++axis) {
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

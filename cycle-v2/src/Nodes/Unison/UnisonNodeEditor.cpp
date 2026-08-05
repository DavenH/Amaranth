#include "UnisonNodeEditor.h"

#include "UnisonNode.h"
#include "../Effects/EffectPreviewRenderer.h"
#include "../../Graph/NodeParameterMap.h"
#include "../../UI/NodeEditorHost.h"

namespace CycleV2 {

namespace {

class UnisonEditorComponent final : public Component {
public:
    UnisonEditorComponent(
            NodeEditorCommands& commandsToUse,
            NodeEditorPresentation& presentationToUse,
            NodeEditorResources& resourcesToUse) :
            commands     (commandsToUse)
        ,   presentation (presentationToUse)
        ,   resources    (resourcesToUse) {
        closeButton.setButtonText(String::fromUTF8("\xc3\x97"));
        closeButton.onClick = [this] { presentation.closeNodeEditor(); };
        addAndMakeVisible(closeButton);

        enabledButton.setButtonText("Enabled");
        enabledButton.onClick = [this] {
            commands.setNodeParameterValue(
                    node.id,
                    "enabled",
                    "Enabled",
                    enabledButton.getToggleState() ? 1.f : 0.f);
        };
        addAndMakeVisible(enabledButton);

        modeSelector.addItem("Group", 1);
        modeSelector.addItem("Individual", 2);
        modeSelector.onChange = [this] { modeChanged(); };
        addAndMakeVisible(modeSelector);

        voiceSelector.onChange = [this] {
            selectedVoice = jmax(0, voiceSelector.getSelectedItemIndex());
            updateIndividualControls();
        };
        addAndMakeVisible(voiceSelector);

        addVoiceButton.setButtonText("+");
        addVoiceButton.onClick = [this] { addIndividualVoice(); };
        addAndMakeVisible(addVoiceButton);

        removeVoiceButton.setButtonText(String::fromUTF8("\xe2\x88\x92"));
        removeVoiceButton.onClick = [this] { removeIndividualVoice(); };
        addAndMakeVisible(removeVoiceButton);

        addControl("order", "Voices", 1.f, ControlKind::GroupParameter);
        addControl("width", "Detune", 35.f, ControlKind::SharedParameter);
        addControl("panSpread", "Pan Spread", 1.f, ControlKind::GroupParameter);
        addControl("phase", "Phase", 0.5f, ControlKind::Phase);
        addControl("jitter", "Jitter", 0.5f, ControlKind::GroupParameter);
        addControl("voiceFine", "Voice Detune", 0.5f, ControlKind::IndividualVoice);
        addControl("voicePan", "Voice Pan", 0.5f, ControlKind::IndividualVoice);
    }

    void setNode(const Node& nodeToUse) {
        bindingNode = true;
        node = nodeToUse;
        const NodeParameterMap parameters(node);
        enabledButton.setToggleState(
                parameters.boolValue("enabled", true),
                dontSendNotification);
        modeSelector.setSelectedId(
                individualMode() ? 2 : 1,
                dontSendNotification);
        for (auto& control : controls) {
            if (control->kind != ControlKind::IndividualVoice) {
                control->slider.setValue(
                        parameters.stringValue(
                                control->id,
                                String(control->defaultValue)).getDoubleValue(),
                        dontSendNotification);
            }
            updateReadout(*control);
        }
        selectedVoice = jlimit(0, jmax(0, individualVoiceCount() - 1), selectedVoice);
        updateModePresentation();
        bindingNode = false;
        repaint();
    }

    void paint(Graphics& graphics) override {
        graphics.fillAll(Colour(0xff11151b));
        graphics.setColour(Colour(0xff2b3340));
        graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.f, 1.f);
        graphics.setColour(Colour(0xffeef2f6));
        graphics.setFont(FontOptions(18.f, Font::bold));
        graphics.drawText("UNISON", 18, 10, getWidth() - 80, 28, Justification::centredLeft);
        if (node.id.isNotEmpty()) {
            paintUnisonPhasePreview(
                    graphics,
                    Rectangle<float>(18.f, 52.f, (float) getWidth() - 36.f, 150.f),
                    node,
                    1.f,
                    resources.unisonPreviewContext());
        }
    }

    void resized() override {
        closeButton.setBounds(getWidth() - 42, 9, 28, 28);
        enabledButton.setBounds(getWidth() - 142, 12, 88, 24);
        modeSelector.setBounds(18, 216, 118, 26);
        voiceSelector.setBounds(146, 216, 126, 26);
        addVoiceButton.setBounds(282, 216, 30, 26);
        removeVoiceButton.setBounds(318, 216, 30, 26);

        int y = 250;
        for (auto& control : controls) {
            if (!control->slider.isVisible()) {
                continue;
            }
            control->label.setBounds(18, y, getWidth() - 112, 16);
            control->readout.setBounds(getWidth() - 110, y, 92, 16);
            control->slider.setBounds(18, y + 18, getWidth() - 36, 24);
            y += 50;
        }
    }

    var automationState() const {
        auto state = std::make_unique<DynamicObject>();
        state->setProperty("kind", "Unison");
        state->setProperty("enabled", enabledButton.getToggleState());
        state->setProperty("mode", individualMode() ? "individual" : "group");
        state->setProperty("selectedVoice", selectedVoice);
        state->setProperty("voiceCount", individualVoiceCount());
        Array<var> values;
        for (const auto& control : controls) {
            auto value = std::make_unique<DynamicObject>();
            value->setProperty("id", control->id);
            value->setProperty("value", control->slider.getValue());
            value->setProperty("readout", control->readout.getText());
            value->setProperty("visible", control->slider.isVisible());
            values.add(var(value.release()));
        }
        state->setProperty("controls", values);
        return var(state.release());
    }

private:
    enum class ControlKind {
        GroupParameter,
        SharedParameter,
        Phase,
        IndividualVoice
    };

    struct Control {
        String id;
        String name;
        float defaultValue {};
        ControlKind kind { ControlKind::SharedParameter };
        Label label;
        Slider slider;
        Label readout;
        bool editing {};
    };

    void addControl(
            const String& id,
            const String& name,
            float defaultValue,
            ControlKind controlKind) {
        auto control = std::make_unique<Control>();
        control->id = id;
        control->name = name;
        control->defaultValue = defaultValue;
        control->kind = controlKind;
        control->label.setText(name, dontSendNotification);
        control->label.setColour(Label::textColourId, Colour(0xffaab4c0));
        control->readout.setJustificationType(Justification::centredRight);
        control->readout.setColour(Label::textColourId, Colour(0xffeef2f6));
        control->slider.setSliderStyle(Slider::LinearHorizontal);
        control->slider.setComponentID("unison." + id + ".slider");
        control->slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        if (id == "order") {
            control->slider.setRange(1.0, CycleDsp::maximumUnisonOrder, 1.0);
        } else if (id == "width") {
            control->slider.setRange(0.0, CycleDsp::maximumUnisonDetuneCents, 0.1);
        } else {
            control->slider.setRange(0.0, 1.0, 0.0001);
        }
        control->slider.setDoubleClickReturnValue(true, defaultValue);
        auto* raw = control.get();
        control->slider.onDragStart = [this, raw] {
            raw->editing = true;
            if (isIndividualVoiceControl(*raw)) {
                commands.beginNodeModelEdit();
            } else {
                commands.beginNodeParameterEdit(
                        node.id,
                        raw->id,
                        raw->name,
                        (float) raw->slider.getValue());
            }
        };
        control->slider.onValueChange = [this, raw] {
            const float value = (float) raw->slider.getValue();
            updateReadout(*raw);
            if (isIndividualVoiceControl(*raw)) {
                setIndividualVoiceValue(raw->id, value);
            } else if (raw->editing) {
                commands.updateNodeParameterEditValue(value);
                setLocalNodeParameter(raw->id, value);
            } else if (!bindingNode) {
                commands.setNodeParameterValue(node.id, raw->id, raw->name, value);
                setLocalNodeParameter(raw->id, value);
            }
            repaint();
        };
        control->slider.onDragEnd = [this, raw] {
            if (isIndividualVoiceControl(*raw)) {
                commands.endNodeModelEdit();
            } else {
                commands.endNodeParameterEdit();
            }
            raw->editing = false;
        };
        addAndMakeVisible(control->label);
        addAndMakeVisible(control->slider);
        addAndMakeVisible(control->readout);
        controls.push_back(std::move(control));
    }

    bool individualMode() const {
        return NodeParameterMap(node).stringValue("mode", "group") == "individual";
    }

    std::shared_ptr<const UnisonNodeModelState> individualModel() const {
        return std::dynamic_pointer_cast<const UnisonNodeModelState>(node.model);
    }

    int individualVoiceCount() const {
        const auto model = individualModel();
        return model != nullptr ? (int) model->voices().size() : 1;
    }

    bool isIndividualVoiceControl(const Control& control) const {
        return individualMode()
                && (control.kind == ControlKind::IndividualVoice
                        || control.kind == ControlKind::Phase);
    }

    void modeChanged() {
        if (bindingNode || node.id.isEmpty()) {
            return;
        }
        const String mode = modeSelector.getSelectedId() == 2 ? "individual" : "group";
        commands.setNodeParameterText(node.id, "mode", "Mode", mode);
        setLocalNodeParameter("mode", mode);
        updateModePresentation();
        repaint();
    }

    void updateModePresentation() {
        const bool individual = individualMode();
        if (!individual) {
            for (auto& control : controls) {
                if (control->id == "phase") {
                    control->slider.setValue(
                            NodeParameterMap(node).floatValue("phase", 0.5f),
                            dontSendNotification);
                    updateReadout(*control);
                    break;
                }
            }
        }
        voiceSelector.clear(dontSendNotification);
        for (int index = 0; index < individualVoiceCount(); ++index) {
            voiceSelector.addItem("Voice " + String(index + 1), index + 1);
        }
        voiceSelector.setSelectedItemIndex(selectedVoice, dontSendNotification);
        voiceSelector.setVisible(individual);
        addVoiceButton.setVisible(individual);
        removeVoiceButton.setVisible(individual);
        addVoiceButton.setEnabled(individualVoiceCount() < CycleDsp::maximumUnisonOrder);
        removeVoiceButton.setEnabled(individualVoiceCount() > 1);
        for (auto& control : controls) {
            const bool visible = control->kind == ControlKind::SharedParameter
                    || control->kind == ControlKind::Phase
                    || (individual
                            ? control->kind == ControlKind::IndividualVoice
                            : control->kind == ControlKind::GroupParameter);
            control->label.setVisible(visible);
            control->slider.setVisible(visible);
            control->readout.setVisible(visible);
        }
        updateIndividualControls();
        resized();
    }

    void updateIndividualControls() {
        const auto model = individualModel();
        if (!individualMode()
                || model == nullptr
                || selectedVoice >= (int) model->voices().size()) {
            return;
        }
        const auto& voice = model->voices()[(size_t) selectedVoice];
        for (auto& control : controls) {
            if (control->id == "voiceFine") {
                control->slider.setValue(voice.detunePosition, dontSendNotification);
            } else if (control->id == "voicePan") {
                control->slider.setValue(voice.pan, dontSendNotification);
            } else if (control->id == "phase") {
                control->slider.setValue(voice.phaseCycles, dontSendNotification);
            } else {
                continue;
            }
            updateReadout(*control);
        }
    }

    void updateReadout(Control& control) {
        const float value = (float) control.slider.getValue();
        String text;
        if (control.id == "order") {
            const int voices = roundToInt(value);
            text = String(voices) + (voices == 1 ? " voice" : " voices");
        } else if (control.id == "width") {
            text = String(value, 1) + " cents";
        } else if (control.id == "phase") {
            text = String(value, 2) + " cycles";
        } else if (control.id == "voiceFine") {
            const float width = NodeParameterMap(node).floatValue("width", 35.f);
            text = String(CycleDsp::UnisonCore::detuneCentsFromPosition(value, width), 1)
                    + " cents";
        } else {
            text = String(roundToInt(value * 100.f)) + "%";
        }
        control.readout.setText(text, dontSendNotification);
    }

    void setLocalNodeParameter(const String& id, float value) {
        setLocalNodeParameter(id, String(value, 6));
    }

    void setLocalNodeParameter(const String& id, const String& value) {
        for (auto& parameter : node.parameters) {
            if (parameter.id == id) {
                parameter.value = value;
                return;
            }
        }
    }

    void publishIndividualVoices(std::vector<UnisonIndividualVoice> voices) {
        const auto current = individualModel();
        const uint64_t revision = current != nullptr ? current->revision() + 1 : 1;
        const auto next = UnisonNodeModelState::create(std::move(voices), revision);
        if (!commands.publishNodeModel(node.id, next)) {
            return;
        }
        node.model = next;
        updateModePresentation();
    }

    void setIndividualVoiceValue(const String& id, float value) {
        const auto model = individualModel();
        if (model == nullptr || selectedVoice >= (int) model->voices().size()) {
            return;
        }
        auto voices = model->voices();
        auto& voice = voices[(size_t) selectedVoice];
        if (id == "voiceFine") {
            voice.detunePosition = value;
        } else if (id == "voicePan") {
            voice.pan = value;
        } else if (id == "phase") {
            voice.phaseCycles = value;
        }
        publishIndividualVoices(std::move(voices));
    }

    void addIndividualVoice() {
        const auto model = individualModel();
        auto voices = model != nullptr
                ? model->voices()
                : std::vector<UnisonIndividualVoice> { {} };
        if (voices.size() >= (size_t) CycleDsp::maximumUnisonOrder) {
            return;
        }
        voices.push_back({});
        selectedVoice = (int) voices.size() - 1;
        publishIndividualVoices(std::move(voices));
    }

    void removeIndividualVoice() {
        const auto model = individualModel();
        if (model == nullptr || model->voices().size() <= 1) {
            return;
        }
        auto voices = model->voices();
        voices.erase(voices.begin() + selectedVoice);
        selectedVoice = jmin(selectedVoice, (int) voices.size() - 1);
        publishIndividualVoices(std::move(voices));
    }

    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    NodeEditorResources& resources;
    Node node;
    TextButton closeButton;
    ToggleButton enabledButton;
    ComboBox modeSelector;
    ComboBox voiceSelector;
    TextButton addVoiceButton;
    TextButton removeVoiceButton;
    int selectedVoice {};
    bool bindingNode {};
    std::vector<std::unique_ptr<Control>> controls;
};

class UnisonNodeEditor final : public NodeEditor {
public:
    explicit UnisonNodeEditor(const NodeEditorContext& context) :
            editor(context.commands, context.presentation, context.resources) {}

    Component& component() override { return editor; }
    void bind(const Node& node) override { editor.setNode(node); }
    void renderOpenGL(float) override {}
    void appendAutomationState(DynamicObject& state) const override {
        state.setProperty("effectParameters", editor.automationState());
    }
    Rectangle<float> panelBoundsForAutomation() const override {
        return editor.getLocalBounds().toFloat();
    }
    void releaseOpenGLResources() override {}

private:
    UnisonEditorComponent editor;
};

class UnisonNodeEditorFactory final : public NodeEditorFactory {
public:
    std::unique_ptr<NodeEditor> create(
            const Node&,
            const NodeEditorContext& context) const override {
        return std::make_unique<UnisonNodeEditor>(context);
    }
};

}

std::unique_ptr<NodeEditorFactory> createUnisonNodeEditorFactory() {
    return std::make_unique<UnisonNodeEditorFactory>();
}

}

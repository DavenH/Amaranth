#include <array>
#include <functional>
#include <initializer_list>
#include <optional>

#include <Audio/CycleDsp/EffectParameterMapping.h>

#include "Graph/NodeParameterMap.h"
#include "Nodes/VoiceContext/Editor/VoiceContextNodeEditor.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/NodePropertyControlBinding.h"

namespace CycleV2 {

namespace {

constexpr int kHeaderHeight = 44;
constexpr int kContentInset = 24;
constexpr int kVoiceLengthValueWidth = 72;

String formatInteger(double value) {
    return String(roundToInt(value));
}

std::optional<double> parseInteger(const String& text) {
    const auto value = parsePropertyNumber(text);
    if (!value.has_value() || !approximatelyEqual(*value, (double) roundToInt(*value))) {
        return std::nullopt;
    }
    return roundToInt(*value);
}

String formatPitch(double value) {
    return String(roundToInt(value)) + " semis";
}

std::optional<double> parsePitch(String text) {
    const std::array<String, 4> suffixes {
        "semitones", "semitone", "semis", "semi"
    };
    for (const String& suffix : suffixes) {
        if (text.trim().endsWithIgnoreCase(suffix)) {
            text = text.trim().dropLastCharacters(suffix.length()).trimEnd();
            break;
        }
    }
    return parseInteger(text);
}

String formatVoiceLength(double unitValue) {
    return formatPropertyReal(CycleDsp::voiceLengthSeconds((float) unitValue)) + " s";
}

std::optional<double> parseVoiceLength(const String& text) {
    const auto seconds = parsePropertyNumber(text, "s");
    if (!seconds.has_value()
            || *seconds < CycleDsp::voiceLengthSeconds(0.f)
            || *seconds > CycleDsp::voiceLengthSeconds(1.f)) {
        return std::nullopt;
    }
    return CycleDsp::voiceLengthUnitValue(*seconds);
}

class VoiceContextEditorComponent final : public Component {
public:
    VoiceContextEditorComponent(
            NodeEditorCommands& commandsToUse,
            NodeEditorPresentation& presentationToUse,
            NodeEditorResources& resourcesToUse) :
            commands       (commandsToUse)
        ,   presentation   (presentationToUse)
        ,   resources      (resourcesToUse)
        ,   octave         (*this, commands, "octave", "Octave")
        ,   voiceLength    (*this, "Voice Length")
        ,   pitch          (*this, commands, "pitch", "Pitch") {
        configureHeader();
        configureSelectors();
        configureSliders();
    }

    void setNode(const Node& nextNode) {
        node = nextNode;
        const NodeParameterMap parameters(node);
        const String domain = parameters.stringValue("domain", "waveform");
        const String oversamplingValue = parameters.stringValue("oversampling", "1x");
        waveform.setToggleState(domain.startsWith("waveform"), dontSendNotification);
        spectral.setToggleState(!domain.startsWith("waveform"), dontSendNotification);
        for (size_t index = 0; index < oversampling.size(); ++index) {
            oversampling[index].setToggleState(
                    oversamplingValue == oversamplingValues[index],
                    dontSendNotification);
        }
        portamento.setToggleState(
                parameters.boolValue("portamento", false),
                dontSendNotification);
        octave.bind(node.id, parameters.floatValue("octave", 0.f));
        pitch.bind(node.id, parameters.floatValue("pitch", 0.f));
        syncVoiceLength();
    }

    void paint(Graphics& graphics) override {
        graphics.fillAll(Colour(0xff11161c));
        graphics.setColour(Colour(0xff34404d));
        graphics.drawRoundedRectangle(
                getLocalBounds().toFloat().reduced(0.5f),
                CanvasChromeMetrics::panelCornerRadius,
                CanvasChromeMetrics::restingBorderWidth);
        graphics.setColour(Colour(0xffe2e8ef));
        graphics.setFont(FontOptions(CanvasChromeMetrics::editorTitleFontSize));
        graphics.drawText(
                "VOICE CONTEXT",
                getLocalBounds().reduced(18, 0).removeFromTop(42),
                Justification::centredLeft);
    }

    void resized() override {
        close.setBounds(getWidth() - 42, 6, 28, 28);
        Rectangle<int> rows = getLocalBounds();
        rows.removeFromTop(kHeaderHeight);
        rows.reduce(kContentInset, 4);
        layoutDomainRow(nextRow(rows));
        octave.setBounds(nextRow(rows));
        layoutVoiceLengthRow(nextRow(rows));
        pitch.setBounds(nextRow(rows));
        layoutOversamplingRow(nextRow(rows));
        layoutToggleRow(nextRow(rows));
    }

    var automationState() const {
        auto* state = new DynamicObject();
        state->setProperty("kind", "VOICE_CONTEXT");
        state->setProperty("domain", waveform.getToggleState() ? "waveform" : "spectral");
        state->setProperty("octave", propertySliderRowAutomationState(octave));
        state->setProperty("voiceLength", propertySliderRowAutomationState(voiceLength));
        state->setProperty("pitch", propertySliderRowAutomationState(pitch));
        state->setProperty("oversampling", selectedOversampling());
        state->setProperty("portamento", portamento.getToggleState());
        state->setProperty(
                "previewVoiceLengthSeconds",
                CycleDsp::voiceLengthSeconds((float) voiceLength.slider.getValue()));
        return state;
    }

private:
    void configureHeader() {
        close.setButtonText(String::fromUTF8("×"));
        close.setComponentID("voiceContextEditor.close");
        close.setTooltip("Close Voice Context editor");
        close.setWantsKeyboardFocus(true);
        close.onClick = [this] {
            presentation.closeNodeEditor();
        };
        addAndMakeVisible(close);
    }

    void configureSelectors() {
        stylePropertyLabel(domainLabel, "Domain");
        stylePropertyLabel(oversamplingLabel, "Oversampling");
        addAndMakeVisible(domainLabel);
        addAndMakeVisible(oversamplingLabel);
        configureDomainSelector();
        configureOversamplingSelector();
        configurePortamento();
    }

    void configureDomainSelector() {
        configureOption(waveform, "Waveform", "voiceContextEditor.domain.waveform", [this] {
            setDomain("waveform");
        });
        configureOption(spectral, "Spectral", "voiceContextEditor.domain.spectral", [this] {
            setDomain("spectral");
        });
    }

    void configureOversamplingSelector() {
        for (size_t index = 0; index < oversampling.size(); ++index) {
            configureOption(
                    oversampling[index],
                    oversamplingValues[index],
                    "voiceContextEditor.oversampling." + oversamplingValues[index],
                    [this, index] { setOversampling(oversamplingValues[index]); });
        }
    }

    void configurePortamento() {
        portamento.setButtonText("Portamento");
        portamento.setComponentID("voiceContextEditor.portamento");
        portamento.setTooltip("Glide continuously between successive pitches.");
        portamento.onClick = [this] {
            commands.setNodeParameterValue(
                    node.id,
                    "portamento",
                    "Portamento",
                    portamento.getToggleState() ? 1.f : 0.f);
        };
        addAndMakeVisible(portamento);
    }

    void configureSliders() {
        configureOctave();
        configureVoiceLength();
        configurePitch();
    }

    void configureOctave() {
        octave.slider.setRange(-2.0, 2.0, 1.0);
        octave.slider.setComponentID("voiceContextEditor.octave");
        octave.value.setComponentID("voiceContextEditor.octave.value");
        octave.configureValuePresentation(
                formatInteger,
                parseInteger,
                0.0,
                1.0,
                1.0,
                "Voice octave offset. Arrow keys select one octave; double-click resets to zero.");
        octave.slider.setLandmarks({
                { -2.0, "-2" }, { -1.0, "-1" }, { 0.0, "0" }, { 1.0, "+1" }, { 2.0, "+2" }
        });
    }

    void configureVoiceLength() {
        voiceLength.slider.setRange(0.0, 1.0, 0.00001);
        voiceLength.slider.setComponentID("voiceContextEditor.voiceLength");
        voiceLength.value.setComponentID("voiceContextEditor.voiceLength.value");
        voiceLength.configureValuePresentation(
                formatVoiceLength,
                parseVoiceLength,
                CycleDsp::voiceLengthUnitValue(1.0),
                0.01,
                0.001,
                "Preview voice duration in seconds. Shift-drag for fine adjustment.");
        voiceLength.slider.setKeyboardStepper([](double current, bool increase, bool fine) {
            const double seconds = CycleDsp::voiceLengthSeconds((float) current);
            const double step = fine ? 0.01 : 0.1;
            return CycleDsp::voiceLengthUnitValue(seconds + (increase ? step : -step));
        });
        voiceLength.slider.setLandmarks({
                { 0.0, "0.05" },
                { CycleDsp::voiceLengthUnitValue(1.0), "1" },
                { CycleDsp::voiceLengthUnitValue(7.0), "7" },
                { 1.0, "148" }
        });
        voiceLength.slider.onValueChange = [this] {
            if (!syncingVoiceLength) {
                resources.setPreviewVoiceLengthSeconds(
                        CycleDsp::voiceLengthSeconds((float) voiceLength.slider.getValue()));
            }
        };
    }

    void configurePitch() {
        pitch.slider.setRange(-12.0, 12.0, 1.0);
        pitch.slider.setComponentID("voiceContextEditor.pitch");
        pitch.value.setComponentID("voiceContextEditor.pitch.value");
        pitch.configureValuePresentation(
                formatPitch,
                parsePitch,
                0.0,
                1.0,
                1.0,
                "Voice pitch offset in semitones. Arrow keys select one semitone.");
        pitch.slider.setLandmarks({ { -12.0, "-12" }, { 0.0, "0" }, { 12.0, "+12" } });
    }

    void configureOption(
            TextButton& button,
            const String& text,
            const String& id,
            std::function<void()> action) {
        stylePropertyButton(button, text);
        button.setComponentID(id);
        button.setClickingTogglesState(false);
        button.setWantsKeyboardFocus(true);
        button.onClick = std::move(action);
        addAndMakeVisible(button);
    }

    void setDomain(const String& value) {
        if (commands.setNodeParameterText(node.id, "domain", "Start Domain", value)) {
            waveform.setToggleState(value == "waveform", dontSendNotification);
            spectral.setToggleState(value == "spectral", dontSendNotification);
        }
    }

    void setOversampling(const String& value) {
        if (!commands.setNodeParameterText(node.id, "oversampling", "Oversampling", value)) {
            return;
        }
        for (size_t index = 0; index < oversampling.size(); ++index) {
            oversampling[index].setToggleState(
                    oversamplingValues[index] == value,
                    dontSendNotification);
        }
    }

    void syncVoiceLength() {
        const ScopedValueSetter<bool> guard(syncingVoiceLength, true);
        voiceLength.slider.setValue(
                CycleDsp::voiceLengthUnitValue(
                        resources.unisonPreviewContext().voiceDurationSeconds),
                dontSendNotification);
        voiceLength.refreshValueText();
    }

    String selectedOversampling() const {
        for (size_t index = 0; index < oversampling.size(); ++index) {
            if (oversampling[index].getToggleState()) {
                return oversamplingValues[index];
            }
        }
        return {};
    }

    static Rectangle<int> nextRow(Rectangle<int>& rows) {
        Rectangle<int> row = rows.removeFromTop(PropertyControlMetrics::rowHeight);
        rows.removeFromTop(PropertyControlMetrics::rowGap);
        return row;
    }

    void layoutDomainRow(Rectangle<int> row) {
        layoutSelectorRow(row, domainLabel, { &waveform, &spectral });
    }

    void layoutVoiceLengthRow(Rectangle<int> row) {
        voiceLength.setBounds(
                row,
                PropertyControlMetrics::labelWidth,
                PropertyControlMetrics::inlineGap,
                kVoiceLengthValueWidth);
    }

    void layoutOversamplingRow(Rectangle<int> row) {
        layoutSelectorRow(
                row,
                oversamplingLabel,
                { &oversampling[0], &oversampling[1], &oversampling[2], &oversampling[3] });
    }

    static void layoutSelectorRow(
            Rectangle<int> row,
            Label& label,
            std::initializer_list<TextButton*> buttons) {
        label.setBounds(row.removeFromLeft(PropertyControlMetrics::labelWidth));
        row.removeFromLeft(PropertyControlMetrics::inlineGap);
        const int gaps = PropertyControlMetrics::rowGap * ((int) buttons.size() - 1);
        const int buttonWidth = (row.getWidth() - gaps) / (int) buttons.size();
        for (TextButton* button : buttons) {
            button->setBounds(row.removeFromLeft(buttonWidth));
            row.removeFromLeft(PropertyControlMetrics::rowGap);
        }
    }

    void layoutToggleRow(Rectangle<int> row) {
        row.removeFromLeft(PropertyControlMetrics::labelWidth
                + PropertyControlMetrics::inlineGap);
        portamento.setBounds(row);
    }

    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    NodeEditorResources& resources;
    Node node;
    TextButton close;
    Label domainLabel;
    TextButton waveform;
    TextButton spectral;
    NodePropertySliderRow octave;
    PropertySliderRow voiceLength;
    NodePropertySliderRow pitch;
    Label oversamplingLabel;
    std::array<TextButton, 4> oversampling;
    const std::array<String, 4> oversamplingValues { "1x", "2x", "4x", "8x" };
    ToggleButton portamento;
    bool syncingVoiceLength {};
};

class VoiceContextNodeEditor final : public NodeEditor {
public:
    explicit VoiceContextNodeEditor(const NodeEditorContext& context) :
            editor(context.commands, context.presentation, context.resources) {}

    Component& component() override { return editor; }
    void bind(const Node& node) override { editor.setNode(node); }
    void renderOpenGL(float) override {}
    void appendAutomationState(DynamicObject& state) const override {
        state.setProperty("voiceContext", editor.automationState());
    }
    Rectangle<float> panelBoundsForAutomation() const override {
        return editor.getLocalBounds().toFloat();
    }
    void releaseOpenGLResources() override {}

private:
    VoiceContextEditorComponent editor;
};

class VoiceContextNodeEditorFactory final : public NodeEditorFactory {
public:
    std::unique_ptr<NodeEditor> create(
            const Node&,
            const NodeEditorContext& context) const override {
        return std::make_unique<VoiceContextNodeEditor>(context);
    }
};

}

std::unique_ptr<NodeEditorFactory> createVoiceContextNodeEditorFactory() {
    return std::make_unique<VoiceContextNodeEditorFactory>();
}

}

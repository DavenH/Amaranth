#include <array>
#include <optional>

#include <Audio/CycleDsp/EffectParameterMapping.h>

#include "Graph/NodeParameterMap.h"
#include "Nodes/VoiceContext/Editor/VoiceContextNodeEditor.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/EditorChromeLayout.h"
#include "UI/Editors/NodePropertyControlBinding.h"
#include "UI/Editors/PropertyControls.h"
#include "UI/Editors/PropertySegmentedSelector.h"

namespace CycleV2 {

namespace {

constexpr int kContentInset = 24;
constexpr int kValueWidth = 72;
constexpr float kLandmarkEndInset = 15.f;
constexpr int kPitchTimingRowCount = 4;
constexpr int kSynthesisRowCount = 3;
constexpr int kGroupContentHeight = 2 * PropertyControlMetrics::groupLabelHeight
        + (kPitchTimingRowCount + kSynthesisRowCount) * PropertyControlMetrics::rowHeight
        + (kPitchTimingRowCount + kSynthesisRowCount - 2)
                * PropertyControlMetrics::rowGap
        + PropertyControlMetrics::sectionGap;

std::vector<PropertySegmentOption> oversamplingOptions() {
    return {
            { "1x", "1x", "voiceContextEditor.oversampling.1x", "1x oversampling" },
            { "2x", "2x", "voiceContextEditor.oversampling.2x", "2x oversampling" },
            { "4x", "4x", "voiceContextEditor.oversampling.4x", "4x oversampling" },
            { "8x", "8x", "voiceContextEditor.oversampling.8x", "8x oversampling" }
    };
}

std::vector<PropertySegmentOption> controlIntervalOptions() {
    return {
            { "16", "16", "voiceContextEditor.controlInterval.16",
                    "Samples between requested synthesis control updates." },
            { "64", "64", "voiceContextEditor.controlInterval.64",
                    "Samples between requested synthesis control updates." },
            { "256", "256", "voiceContextEditor.controlInterval.256",
                    "Samples between requested synthesis control updates." },
            { "1024", "1024", "voiceContextEditor.controlInterval.1024",
                    "Samples between requested synthesis control updates." }
    };
}

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
            NodeEditorPresentation& presentationToUse) :
            commands       (commandsToUse)
        ,   presentation   (presentationToUse)
        ,   octave         (*this, commands, "octave", "Octave")
        ,   voiceLength    (*this, commands, "voiceLength", "Voice Length")
        ,   pitch          (*this, commands, "pitch", "Pitch") {
        configureHeader();
        configureSelectors();
        configureSliders();
    }

    void setNode(const Node& nextNode) {
        node = nextNode;
        const NodeParameterMap parameters(node);
        const String oversamplingValue = parameters.stringValue("oversampling", "1x");
        const String controlIntervalValue = parameters.stringValue("controlInterval", "16");
        oversamplingSelector.setSelectedValue(oversamplingValue);
        controlIntervalSelector.setSelectedValue(controlIntervalValue);
        portamento.setToggleState(
                parameters.boolValue("portamento", false),
                dontSendNotification);
        pitchIndependentSpectralControl.setToggleState(
                parameters.boolValue("pitchIndependentSpectralControl", false),
                dontSendNotification);
        octave.bind(node.id, parameters.floatValue("octave", 0.f));
        voiceLength.bind(
                node.id,
                parameters.floatValue(
                        "voiceLength",
                        CycleDsp::voiceLengthUnitValue(1.0)));
        pitch.bind(node.id, parameters.floatValue("pitch", 0.f));
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
        const auto header = fullEditorHeaderLayout(getLocalBounds(), false);
        graphics.drawText("VOICE CONTEXT", header.title, Justification::centredLeft);
    }

    void resized() override {
        const auto header = fullEditorHeaderLayout(getLocalBounds(), false);
        close.setBounds(header.close);
        Rectangle<int> rows = getLocalBounds();
        rows.removeFromTop(header.header.getHeight());
        rows.reduce(kContentInset, 4);
        rows.setY(rows.getY() + jmax(0, (rows.getHeight() - kGroupContentHeight) / 2));
        rows.setHeight(kGroupContentHeight);

        pitchTimingGroup.setBounds(
                rows.removeFromTop(PropertyControlMetrics::groupLabelHeight));
        layoutSliderRow(octave, nextRow(rows, true));
        layoutSliderRow(pitch, nextRow(rows, true));
        layoutVoiceLengthRow(nextRow(rows, true));
        layoutToggleRow(nextRow(rows, false), portamento);

        rows.removeFromTop(PropertyControlMetrics::sectionGap);
        synthesisGroup.setBounds(
                rows.removeFromTop(PropertyControlMetrics::groupLabelHeight));
        layoutOversamplingRow(nextRow(rows, true));
        layoutControlIntervalRow(nextRow(rows, true));
        layoutToggleRow(nextRow(rows, false), pitchIndependentSpectralControl);
    }

    var automationState() const {
        auto* state = new DynamicObject();
        state->setProperty("kind", "VOICE_CONTEXT");
        state->setProperty(
                "pitchTimingGroup",
                propertyGroupLabelAutomationState(pitchTimingGroup));
        state->setProperty(
                "synthesisGroup",
                propertyGroupLabelAutomationState(synthesisGroup));
        state->setProperty("octave", propertySliderRowAutomationState(octave));
        state->setProperty("voiceLength", propertySliderRowAutomationState(voiceLength));
        state->setProperty("pitch", propertySliderRowAutomationState(pitch));
        state->setProperty("oversampling", oversamplingSelector.selectedValue());
        state->setProperty("controlInterval", controlIntervalSelector.selectedValue());
        state->setProperty("portamento", portamento.getToggleState());
        state->setProperty("pitchIndependentSpectralControl",
                pitchIndependentSpectralControl.getToggleState());
        state->setProperty(
                "previewVoiceLengthSeconds",
                CycleDsp::voiceLengthSeconds((float) voiceLength.slider.getValue()));
        state->setProperty(
                "voiceLengthSeconds",
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
        addAndMakeVisible(pitchTimingGroup);
        addAndMakeVisible(synthesisGroup);
    }

    void configureSelectors() {
        stylePropertyLabel(oversamplingLabel, "Oversampling");
        stylePropertyLabel(controlIntervalLabel, "Control interval");
        oversamplingSelector.setComponentID("voiceContextEditor.oversampling");
        controlIntervalSelector.setComponentID("voiceContextEditor.controlInterval");
        addAndMakeVisible(oversamplingLabel);
        addAndMakeVisible(controlIntervalLabel);
        addAndMakeVisible(oversamplingSelector);
        addAndMakeVisible(controlIntervalSelector);
        oversamplingSelector.onChange = [this](const String& value) {
            setOversampling(value);
        };
        controlIntervalSelector.onChange = [this](const String& value) {
            setControlInterval(value);
        };
        configurePortamento();
        configurePitchIndependentSpectralControl();
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

    void configurePitchIndependentSpectralControl() {
        pitchIndependentSpectralControl.setButtonText("Pitch-independent spectral control");
        pitchIndependentSpectralControl.setComponentID(
                "voiceContextEditor.pitchIndependentSpectralControl");
        pitchIndependentSpectralControl.setTooltip(
                "Render spectral frames at the control interval regardless of note pitch.");
        pitchIndependentSpectralControl.onClick = [this] {
            commands.setNodeParameterValue(
                    node.id,
                    "pitchIndependentSpectralControl",
                    "Pitch-independent spectral control",
                    pitchIndependentSpectralControl.getToggleState() ? 1.f : 0.f);
        };
        addAndMakeVisible(pitchIndependentSpectralControl);
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
        octave.slider.setTrackEndInset(kLandmarkEndInset);
        octave.setValueJustification(Justification::centredLeft);
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
                "Voice duration in seconds. Shift-drag for fine adjustment.");
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
        voiceLength.slider.setTrackEndInset(kLandmarkEndInset);
        voiceLength.setValueJustification(Justification::centredLeft);
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
        pitch.slider.setTrackEndInset(kLandmarkEndInset);
        pitch.setValueJustification(Justification::centredLeft);
    }

    void setOversampling(const String& value) {
        if (!commands.setNodeParameterText(node.id, "oversampling", "Oversampling", value)) {
            oversamplingSelector.setSelectedValue(
                    NodeParameterMap(node).stringValue("oversampling", "1x"));
        }
    }

    void setControlInterval(const String& value) {
        if (!commands.setNodeParameterText(
                    node.id,
                    "controlInterval",
                    "Control Interval",
                    value)) {
            controlIntervalSelector.setSelectedValue(
                    NodeParameterMap(node).stringValue("controlInterval", "16"));
        }
    }
    static Rectangle<int> nextRow(Rectangle<int>& rows, bool hasFollowingRow) {
        Rectangle<int> row = rows.removeFromTop(PropertyControlMetrics::rowHeight);
        if (hasFollowingRow) {
            rows.removeFromTop(PropertyControlMetrics::rowGap);
        }
        return row;
    }

    void layoutVoiceLengthRow(Rectangle<int> row) {
        voiceLength.setBounds(
                row,
                PropertyControlMetrics::labelWidth,
                PropertyControlMetrics::inlineGap,
                kValueWidth);
    }

    static void layoutSliderRow(PropertySliderRow& slider, Rectangle<int> row) {
        slider.setBounds(
                row,
                PropertyControlMetrics::labelWidth,
                PropertyControlMetrics::inlineGap,
                kValueWidth);
    }

    void layoutOversamplingRow(Rectangle<int> row) {
        layoutSelectorRow(row, oversamplingLabel, oversamplingSelector);
    }

    void layoutControlIntervalRow(Rectangle<int> row) {
        layoutSelectorRow(row, controlIntervalLabel, controlIntervalSelector);
    }

    static void layoutSelectorRow(
            Rectangle<int> row,
            Label& label,
            PropertySegmentedSelector& selector) {
        label.setBounds(row.removeFromLeft(PropertyControlMetrics::labelWidth));
        row.removeFromLeft(PropertyControlMetrics::inlineGap);
        selector.setBounds(row);
    }

    static void layoutToggleRow(Rectangle<int> row, ToggleButton& toggle) {
        row.removeFromLeft(PropertyControlMetrics::labelWidth
                + PropertyControlMetrics::inlineGap);
        toggle.setBounds(row);
    }

    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    Node node;
    TextButton close;
    PropertyGroupLabel pitchTimingGroup { "Pitch & timing" };
    PropertyGroupLabel synthesisGroup { "Synthesis" };
    NodePropertySliderRow octave;
    NodePropertySliderRow voiceLength;
    NodePropertySliderRow pitch;
    Label oversamplingLabel;
    PropertySegmentedSelector oversamplingSelector { oversamplingOptions() };
    Label controlIntervalLabel;
    PropertySegmentedSelector controlIntervalSelector { controlIntervalOptions() };
    ToggleButton portamento;
    ToggleButton pitchIndependentSpectralControl;
};

class VoiceContextNodeEditor final : public NodeEditor {
public:
    explicit VoiceContextNodeEditor(const NodeEditorContext& context) :
            editor(context.commands, context.presentation) {}

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

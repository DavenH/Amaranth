#include "Nodes/ImpulseResponse/Editor/ImpulseResponseEditorComponent.h"

#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Runtime/MessageThreadWorker.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/PropertyControls.h"

#include <Audio/CycleDsp/IrModel.h>

#include <cmath>
#include <cstdlib>

namespace CycleV2 {

namespace {

constexpr float kControlRailWidth = 348.f;
constexpr int kValueWidth = 72;
constexpr int kActionButtonHeight = 24;
constexpr int kActionButtonWidth = 72;
constexpr int kToggleControlInset = 12;
constexpr float kSampleLabelWidth = 56.f;
constexpr float kSampleLabelHeight = 14.f;
constexpr double kReferenceSampleRate = 44100.0;

struct SampleLandmark {
    float fraction {};
    int sample {};
    float x {};
    Rectangle<float> labelBounds;
};

std::optional<double> parseNumber(String text) {
    text = text.trim();
    if (text.isEmpty()) {
        return std::nullopt;
    }
    const char* start = text.toRawUTF8();
    char* end {};
    const double value = std::strtod(start, &end);
    if (end == start || *end != '\0' || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

String formatSize(double value) {
    return String(CycleDsp::irImpulseLength(value)) + " smp";
}

std::optional<double> parseSize(String text) {
    text = text.trim();
    for (const String& suffix : { String("samples"), String("sample"), String("smp") }) {
        if (text.endsWithIgnoreCase(suffix)) {
            text = text.dropLastCharacters(suffix.length()).trimEnd();
            break;
        }
    }
    const auto parsed = parseNumber(text);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    const int length = roundToInt(*parsed);
    if (!approximatelyEqual(*parsed, (double) length)
            || length < 128 || length > 16384
            || (length & (length - 1)) != 0) {
        return std::nullopt;
    }
    return CycleDsp::irImpulseLengthValue(length);
}

String formatPostGain(double value) {
    const float decibels = CycleDsp::irPostGainDecibels(value);
    return (decibels > 0.f ? "+" : "") + formatPropertyReal(decibels) + " dB";
}

std::optional<double> parsePostGain(String text) {
    text = text.trim();
    if (text.endsWithIgnoreCase("db")) {
        text = text.dropLastCharacters(2).trimEnd();
    }
    const auto decibels = parseNumber(text);
    if (!decibels.has_value()) {
        return std::nullopt;
    }
    const float minimum = CycleDsp::irPostGainDecibels(0.0);
    const float maximum = CycleDsp::irPostGainDecibels(1.0);
    if (*decibels < minimum || *decibels > maximum) {
        return std::nullopt;
    }
    return CycleDsp::irPostGainValueForDecibels((float) *decibels);
}

String formatHighPass(double value) {
    return formatPropertyFrequency(
            CycleDsp::irPrefilterFrequency(value, kReferenceSampleRate));
}

std::optional<double> parseHighPass(const String& text) {
    const auto frequency = parsePropertyFrequency(
            text,
            0.0,
            kReferenceSampleRate * 0.5);
    if (!frequency.has_value()) {
        return std::nullopt;
    }
    return CycleDsp::irPrefilterValueForFrequency(
            (float) *frequency,
            kReferenceSampleRate);
}

void configureSizeControl(LabeledParameterSlider& control) {
    control.slider.setRange(0.0, 1.0, 1.0 / 7.0);
    control.slider.setComponentID("irEditor.size");
    control.value.setComponentID("irEditor.size.value");
    control.configureValuePresentation(
            formatSize,
            parseSize,
            CycleDsp::irImpulseLengthValue(1024),
            1.0 / 7.0,
            1.0 / 7.0,
            "Impulse length in samples: eight power-of-two sizes from 128 to 16384.");
}

void configurePostGainControl(LabeledParameterSlider& control) {
    control.slider.setRange(0.0, 1.0, 0.00001);
    control.slider.setComponentID("irEditor.postGain");
    control.value.setComponentID("irEditor.postGain.value");
    control.configureValuePresentation(
            formatPostGain,
            parsePostGain,
            0.5,
            0.01,
            0.001,
            "Output gain in decibels. Shift-drag for fine adjustment; double-click for 0 dB.");
    control.slider.setKeyboardStepper([](double current, bool increase, bool fine) {
        const float decibels = CycleDsp::irPostGainDecibels(current);
        const float step = fine ? 0.1f : 1.f;
        return CycleDsp::irPostGainValueForDecibels(
                decibels + (increase ? step : -step));
    });
}

void configureHighPassControl(LabeledParameterSlider& control) {
    control.slider.setRange(0.0, 1.0, 0.00001);
    control.slider.setComponentID("irEditor.highPass");
    control.value.setComponentID("irEditor.highPass.value");
    control.configureValuePresentation(
            formatHighPass,
            parseHighPass,
            0.0,
            100.0,
            10.0,
            "High-pass cutoff at the 44.1 kHz reference sample rate. Shift-drag for fine adjustment.");
    control.slider.setKeyboardStepper([](double current, bool increase, bool fine) {
        const float frequency = CycleDsp::irPrefilterFrequency(
                current,
                kReferenceSampleRate);
        const float step = fine ? 10.f : 100.f;
        return CycleDsp::irPrefilterValueForFrequency(
                jlimit(
                        0.f,
                        (float) (kReferenceSampleRate * 0.5),
                        frequency + (increase ? step : -step)),
                kReferenceSampleRate);
    });
}

Rectangle<float> sampleTickLabelBounds(
        Rectangle<float> panel,
        Rectangle<float> labelLimits,
        float tickX) {
    const float width = jmin(kSampleLabelWidth, labelLimits.getWidth());
    const float x = jlimit(
            labelLimits.getX(),
            labelLimits.getRight() - width,
            tickX - width * 0.5f);
    return { x, panel.getBottom() + 5.f, width, kSampleLabelHeight };
}

Justification sampleTickLabelJustification(
        Rectangle<float> labelBounds,
        float tickX,
        float labelWidth) {
    if (labelBounds.getX() > tickX - labelWidth * 0.5f) {
        return Justification::centredLeft;
    }
    if (labelBounds.getRight() < tickX + labelWidth * 0.5f) {
        return Justification::centredRight;
    }
    return Justification::centred;
}

std::vector<SampleLandmark> buildSampleLandmarks(
        int sampleCount,
        Rectangle<float> panel,
        Rectangle<float> labelLimits,
        const std::vector<CurvePanelGridLine>& gridLines) {
    std::vector<SampleLandmark> result;
    result.reserve(gridLines.size());
    for (const auto& gridLine : gridLines) {
        const float fraction = jlimit(
                0.f,
                1.f,
                CycleDsp::irSampleFractionAtDomainPosition(gridLine.domainX));
        const float tickX = panel.getX() + gridLine.panelX;
        const Rectangle<float> label = sampleTickLabelBounds(panel, labelLimits, tickX);
        result.push_back({ fraction, roundToInt(sampleCount * fraction), tickX, label });
    }
    return result;
}

Array<var> sampleLandmarkAutomation(const std::vector<SampleLandmark>& landmarks) {
    Array<var> result;
    for (const auto& landmark : landmarks) {
        auto* encoded = new DynamicObject();
        encoded->setProperty("fraction", landmark.fraction);
        encoded->setProperty("sample", landmark.sample);
        encoded->setProperty("x", landmark.x);
        encoded->setProperty("labelX", landmark.labelBounds.getX());
        encoded->setProperty("labelWidth", landmark.labelBounds.getWidth());
        result.add(var(encoded));
    }
    return result;
}

}

struct ImpulseResponseEditorComponent::Impl {
    explicit Impl(Component& owner) :
            enabled     (owner, "Enabled")
        ,   size        (owner, "Size")
        ,   postGain    (owner, "Post Gain")
        ,   highPass    (owner, "High Pass") {
        enabled.button.setComponentID("irEditor.enabled");
        resourceTitle.setText("IR sample", dontSendNotification);
        resourceTitle.setFont(FontOptions(
                CanvasChromeMetrics::labelFontSize).withStyle("Bold"));
        resourceTitle.setColour(Label::textColourId, Colour(0xff8793a1));
        resourceTitle.setJustificationType(Justification::centredLeft);
        owner.addAndMakeVisible(resourceTitle);

        configureButton(loadAudio, "Load", "irEditor.loadAudio");
        configureButton(modelAudio, "Model", "irEditor.modelAudio");
        configureButton(unload, "Unload", "irEditor.unloadAudio");
        unload.setColour(TextButton::textColourOffId, Colour(0xff9ca8b5));
        owner.addAndMakeVisible(loadAudio);
        owner.addAndMakeVisible(modelAudio);
        owner.addAndMakeVisible(unload);
    }

    static void configureButton(TextButton& button, const String& text, const String& id) {
        stylePropertyButton(button, text);
        button.setComponentID(id);
        button.setWantsKeyboardFocus(true);
        button.setMouseCursor(MouseCursor::PointingHandCursor);
    }

    ParameterToggle enabled;
    LabeledParameterSlider size;
    LabeledParameterSlider postGain;
    LabeledParameterSlider highPass;
    Label resourceTitle;
    TextButton loadAudio;
    TextButton modelAudio;
    TextButton unload;
    std::unique_ptr<FileChooser> chooser;
    MessageThreadWorker worker;
    bool busy {};
    std::optional<ImpulseResponseImportMode> busyMode;
    String error;
};

ImpulseResponseEditorComponent::ImpulseResponseEditorComponent(CurveEditorWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    configureSizeControl(impl->size);
    configurePostGainControl(impl->postGain);
    configureHighPassControl(impl->highPass);
    bindDiscreteControl(impl->enabled);
    bindContinuousControls({ &impl->size, &impl->postGain, &impl->highPass });
    impl->loadAudio.onClick = [this] {
        chooseAudio(ImpulseResponseImportMode::Direct);
    };
    impl->modelAudio.onClick = [this] {
        chooseAudio(ImpulseResponseImportMode::Modelled);
    };
    impl->unload.onClick = [this] {
        if (!removeAudioResource()) {
            impl->error = "The embedded audio could not be unloaded.";
        } else {
            impl->error = {};
        }
        setStatusMessage(impl->error);
        updateResourceState();
    };
}

ImpulseResponseEditorComponent::~ImpulseResponseEditorComponent() = default;

Rectangle<float> ImpulseResponseEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromRight(kControlRailWidth);
}

Rectangle<float> ImpulseResponseEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromRight(kControlRailWidth);
    return bounds.reduced(12.f, 26.f);
}

void ImpulseResponseEditorComponent::paintEditor(Graphics& graphics) {
    const Rectangle<float> panel = editorPanelBounds();
    const int sampleCount = CycleDsp::irImpulseLength(impl->size.slider.getValue());
    graphics.setColour(Colour(0xff8793a1));
    graphics.setFont(FontOptions(10.f));
    const auto landmarks = buildSampleLandmarks(
            sampleCount,
            panel,
            getLocalBounds().toFloat(),
            widget.verticalMajorGridLines());
    for (const auto& landmark : landmarks) {
        graphics.drawVerticalLine(
                roundToInt(landmark.x),
                panel.getBottom(),
                panel.getBottom() + 4.f);
        graphics.drawText(
                String(landmark.sample),
                landmark.labelBounds,
                sampleTickLabelJustification(
                        landmark.labelBounds,
                        landmark.x,
                        landmark.labelBounds.getWidth()));
    }
}

void ImpulseResponseEditorComponent::layoutEditor() {
    Rectangle<int> bounds = editorControlBounds().toNearestInt().reduced(12, 12);
    impl->enabled.setBounds(
            bounds.removeFromTop(PropertyControlMetrics::rowHeight),
            PropertyControlMetrics::labelWidth,
            PropertyControlMetrics::inlineGap + kToggleControlInset);
    bounds.removeFromTop(PropertyControlMetrics::rowGap);
    for (auto* control : { &impl->size, &impl->postGain, &impl->highPass }) {
        control->setBounds(
                bounds.removeFromTop(PropertyControlMetrics::rowHeight),
                PropertyControlMetrics::labelWidth,
                PropertyControlMetrics::inlineGap,
                kValueWidth);
        bounds.removeFromTop(PropertyControlMetrics::rowGap);
    }
    bounds.removeFromTop(PropertyControlMetrics::sectionGap);
    impl->resourceTitle.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(PropertyControlMetrics::rowGap);
    auto actionRow = bounds.removeFromTop(PropertyControlMetrics::rowHeight);
    actionRow = actionRow.withTrimmedTop(
            (PropertyControlMetrics::rowHeight - kActionButtonHeight) / 2);
    impl->loadAudio.setBounds(
            actionRow.removeFromLeft(kActionButtonWidth).withHeight(kActionButtonHeight));
    actionRow.removeFromLeft(PropertyControlMetrics::inlineGap);
    impl->modelAudio.setBounds(
            actionRow.removeFromLeft(kActionButtonWidth).withHeight(kActionButtonHeight));
    actionRow.removeFromLeft(PropertyControlMetrics::inlineGap);
    impl->unload.setBounds(
            actionRow.removeFromLeft(kActionButtonWidth).withHeight(kActionButtonHeight));
}

void ImpulseResponseEditorComponent::syncEditorFromNode() {
    ImpulseResponseNodeModel model;
    model.syncFromNode(node);
    impl->enabled.button.setToggleState(model.enabled, dontSendNotification);
    impl->size.slider.setValue(model.size, dontSendNotification);
    impl->postGain.slider.setValue(model.postGain, dontSendNotification);
    impl->highPass.slider.setValue(model.highPass, dontSendNotification);
    impl->size.refreshValueText();
    impl->postGain.refreshValueText();
    impl->highPass.refreshValueText();
    updateResourceState();
}

void ImpulseResponseEditorComponent::chooseAudio(ImpulseResponseImportMode mode) {
    impl->busy = true;
    impl->busyMode = mode;
    impl->error = {};
    setStatusMessage({});
    updateResourceState();
    impl->chooser = std::make_unique<FileChooser>(
            mode == ImpulseResponseImportMode::Direct
                    ? "Load impulse response audio"
                    : "Model impulse response audio",
            File(),
            "*.wav;*.aif;*.aiff;*.mp3;*.ogg");
    Component::SafePointer<ImpulseResponseEditorComponent> safeThis(this);
    impl->chooser->launchAsync(
            FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
            [safeThis, mode](const FileChooser& chooser) {
                if (safeThis == nullptr) {
                    return;
                }
                const File selected = chooser.getResult();
                if (selected == File()) {
                    safeThis->impl->busy = false;
                    safeThis->impl->busyMode.reset();
                    safeThis->setStatusMessage({});
                    safeThis->updateResourceState();
                    return;
                }
                safeThis->prepareAudio(selected, mode);
            });
}

void ImpulseResponseEditorComponent::prepareAudio(
        const File& file,
        ImpulseResponseImportMode mode) {
    struct PendingPreparation {
        PreparedImpulseResponseAudio prepared;
        String error;
    };
    const Node nodeSnapshot = node;
    auto pending = std::make_shared<PendingPreparation>();
    Component::SafePointer<ImpulseResponseEditorComponent> safeThis(this);
    impl->worker.post(
            [pending, file, mode, nodeSnapshot] {
                const Result result = ImpulseResponseResourcePreparation::prepare(
                        file,
                        mode,
                        nodeSnapshot,
                        pending->prepared);
                pending->error = result.getErrorMessage();
                return true;
            },
            [safeThis, pending] {
                if (safeThis == nullptr) {
                    return;
                }
                safeThis->impl->busy = false;
                safeThis->impl->busyMode.reset();
                if (pending->error.isNotEmpty()) {
                    safeThis->impl->error = pending->error;
                } else if (!safeThis->setAudioResource(std::move(pending->prepared.edit))) {
                    safeThis->impl->error = "The prepared audio could not be applied.";
                } else {
                    safeThis->impl->error = {};
                }
                safeThis->setStatusMessage(safeThis->impl->error);
                safeThis->updateResourceState();
            });
}

void ImpulseResponseEditorComponent::updateResourceState() {
    const auto summary = audioResourceSummary();
    impl->loadAudio.setButtonText(
            impl->busyMode == ImpulseResponseImportMode::Direct ? "Loading…" : "Load");
    impl->modelAudio.setButtonText(
            impl->busyMode == ImpulseResponseImportMode::Modelled ? "Modelling…" : "Model");
    impl->loadAudio.setEnabled(!impl->busy);
    impl->modelAudio.setEnabled(!impl->busy);
    impl->unload.setEnabled(!impl->busy && summary.has_value());
}

void ImpulseResponseEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(
            impl->enabled.button.getToggleState(),
            static_cast<float>(impl->size.slider.getValue()),
            static_cast<float>(impl->postGain.slider.getValue()),
            static_cast<float>(impl->highPass.slider.getValue()),
            0);
}

std::vector<NodeParameter> ImpulseResponseEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    addEditorParameter(result, node, "enabled", "Enabled", impl->enabled.button.getToggleState() ? "1" : "0");
    addEditorParameter(result, node, "size", "Size", String(impl->size.slider.getValue(), 8));
    addEditorParameter(result, node, "post", "Post Gain", String(impl->postGain.slider.getValue(), 8));
    addEditorParameter(result, node, "highPass", "High Pass", String(impl->highPass.slider.getValue(), 8));
    return result;
}

void ImpulseResponseEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.button.getToggleState());
    state.setProperty("size", impl->size.slider.getValue());
    state.setProperty("postGain", impl->postGain.slider.getValue());
    state.setProperty("highPass", impl->highPass.slider.getValue());
    state.setProperty("sizeLayout", propertySliderRowAutomationState(impl->size));
    state.setProperty("postGainLayout", propertySliderRowAutomationState(impl->postGain));
    state.setProperty("highPassLayout", propertySliderRowAutomationState(impl->highPass));
    state.setProperty(
            "landmarks",
            sampleLandmarkAutomation(buildSampleLandmarks(
                CycleDsp::irImpulseLength(impl->size.slider.getValue()),
                editorPanelBounds(),
                getLocalBounds().toFloat(),
                widget.verticalMajorGridLines())));
    const auto summary = audioResourceSummary();
    state.setProperty("resourceActionsAvailable", true);
    state.setProperty("resourceBusy", impl->busy);
    state.setProperty("resourceSectionLabel", impl->resourceTitle.getText());
    state.setProperty("resourceSublabelVisible", false);
    state.setProperty("resourceError", impl->error);
    state.setProperty("resourceBound", summary.has_value());
    state.setProperty("resourceMode", summary.has_value() ? summary->mode : String());
}

}

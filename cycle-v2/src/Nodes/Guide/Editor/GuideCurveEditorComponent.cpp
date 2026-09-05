#include "Nodes/Guide/Editor/GuideCurveEditorComponent.h"

#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Guide/GuideHeatmapLoader.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/EffectEnableButton.h"

#include "Graph/NodeParameterMap.h"

namespace CycleV2 {

namespace {

constexpr float kControlRailWidth = 336.f;
constexpr float kMaximumHostWidth = 1265.f;
constexpr float kMaximumHostHeight = 476.f;
constexpr int kActionButtonHeight = 24;
constexpr int kActionButtonWidth = 72;
constexpr int kSliderValueGap = 4;
constexpr int kSliderValueWidth = 48;

}

struct GuideCurveEditorComponent::Impl {
    explicit Impl(Component& owner) :
            enabled     ("Guide enabled",
                         "Toggles this Guide curve",
                         "Enable or disable this Guide curve")
        ,   noise       (owner, "Noise")
        ,   dcOffset    (owner, "DC Offset")
        ,   phase       (owner, "Phase") {
        imageTitle.setText("Guide image", dontSendNotification);
        imageTitle.setFont(FontOptions(
                CanvasChromeMetrics::labelFontSize).withStyle("Bold"));
        imageTitle.setColour(Label::textColourId, Colour(0xff8793a1));
        imageTitle.setJustificationType(Justification::centredLeft);
        owner.addAndMakeVisible(imageTitle);

        configureButton(
                loadButton,
                "Load",
                "guideEditor.loadImage",
                "Load Guide image",
                "Choose a PNG or JPEG image for this Guide");
        configureButton(
                clearButton,
                "Clear",
                "guideEditor.clearImage",
                "Clear Guide image",
                "Remove the image and use the authored Guide curve directly");
        owner.addAndMakeVisible(loadButton);
        owner.addAndMakeVisible(clearButton);
    }

    static void configureButton(
            TextButton& button,
            const String& text,
            const String& id,
            const String& title,
            const String& description) {
        stylePropertyButton(button, text);
        button.setName(title);
        button.setTitle(title);
        button.setDescription(description);
        button.setTooltip(description);
        button.setComponentID(id);
        button.setWantsKeyboardFocus(true);
        button.setMouseCursor(MouseCursor::PointingHandCursor);
    }

    EffectEnableButton enabled;
    LabeledParameterSlider noise;
    LabeledParameterSlider dcOffset;
    LabeledParameterSlider phase;
    Label imageTitle;
    TextButton loadButton;
    TextButton clearButton;
    std::unique_ptr<FileChooser> chooser;
    GuideHeatmapLoader loader;
};

GuideCurveEditorComponent::GuideCurveEditorComponent(CurveEditorWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    struct ControlSetup {
        LabeledParameterSlider* control;
        const char* id;
        const char* help;
    };
    for (const auto& setup : {
            ControlSetup {
                    &impl->noise,
                    "noise",
                    "Random noise depth. Shift-drag for fine adjustment; double-click to reset."
            },
            ControlSetup {
                    &impl->dcOffset,
                    "dcOffset",
                    "Random DC offset range. Shift-drag for fine adjustment; double-click to reset."
            },
            ControlSetup {
                    &impl->phase,
                    "phase",
                    "Random phase range. Shift-drag for fine adjustment; double-click to reset."
            } }) {
        setup.control->slider.setRange(0.0, 1.0, 0.00001);
        setup.control->slider.setComponentID("guideEditor." + String(setup.id));
        setup.control->value.setComponentID("guideEditor." + String(setup.id) + ".value");
        setup.control->configureValuePresentation(
                formatPropertyPercentage,
                parsePropertyPercentage,
                0.0,
                0.01,
                0.001,
                setup.help);
    }
    impl->enabled.setComponentID("guideEditor.enabled");
    setHeaderAction(impl->enabled);
    bindDiscreteAction(impl->enabled, [] {});
    bindContinuousControls({ &impl->noise, &impl->dcOffset, &impl->phase });

    impl->loadButton.onClick = [this] {
        impl->chooser = std::make_unique<FileChooser>(
                "Choose a Guide heatmap",
                File {},
                "*.png;*.jpg;*.jpeg");
        impl->chooser->launchAsync(
                FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                [safeThis = SafePointer<GuideCurveEditorComponent>(this)](const FileChooser& chooser) {
                    if (safeThis == nullptr) {
                        return;
                    }
                    const File file = chooser.getResult();
                    if (!file.existsAsFile()) {
                        return;
                    }
                    safeThis->impl->loadButton.setEnabled(false);
                    safeThis->impl->clearButton.setEnabled(false);
                    safeThis->impl->loadButton.setButtonText("Loading…");
                    safeThis->setStatusMessage("Loading Guide image…");
                    auto completion = [safeThis](GuideHeatmapAssetPtr asset, String error) mutable {
                        if (safeThis == nullptr) {
                            return;
                        }
                        safeThis->updateHeatmapControls();
                        if (asset == nullptr) {
                            safeThis->setStatusMessage(error);
                            return;
                        }
                        if (safeThis->loadHeatmap == nullptr
                                || !safeThis->loadHeatmap(
                                        safeThis->guide.id,
                                        asset,
                                        safeThis->guide.revision)) {
                            safeThis->setStatusMessage(
                                    "The Guide changed while the image was loading.");
                            return;
                        }
                        safeThis->setStatusMessage({});
                    };
                    safeThis->impl->loader.load(file, std::move(completion));
                });
    };
    impl->clearButton.onClick = [this] {
        if (clearHeatmap != nullptr && clearHeatmap(guide.id, guide.revision)) {
            impl->clearButton.setEnabled(false);
            setStatusMessage({});
        }
    };
}

GuideCurveEditorComponent::~GuideCurveEditorComponent() = default;

Rectangle<float> GuideCurveEditorComponent::preferredHostBounds(Rectangle<float> canvasBounds) {
    Rectangle<float> available = canvasBounds.reduced(36.f, 24.f);
    return available.withSizeKeepingCentre(
                    jmin(available.getWidth(), kMaximumHostWidth),
                    jmin(available.getHeight(), kMaximumHostHeight))
            .withCentre(available.getCentre());
}

void GuideCurveEditorComponent::setGuideResource(
        const GuideCurveResource& nextGuide,
        const GuideHeatmapAssetPtr& nextHeatmap) {
    guide = nextGuide;
    heatmap = nextHeatmap;
    setEditorModelState(guide.model);
    widget.syncFromGuideResource(guide, heatmap);
    const ScopedValueSetter<bool> guard(syncingControls, true);
    syncEditorFromNode();
    applyEditorStateToWidget();
    refreshEditorSubject();
}

void GuideCurveEditorComponent::setHeatmapActions(
        std::function<bool(const String&, GuideHeatmapAssetPtr, uint64_t)> loadAction,
        std::function<bool(const String&, uint64_t)> clearAction) {
    loadHeatmap = std::move(loadAction);
    clearHeatmap = std::move(clearAction);
}

void GuideCurveEditorComponent::renderOpenGL(float scaleFactor) {
    widget.renderGuideExpandedPanelOpenGL(
            editorPanelBounds().translated((float) getX(), (float) getY()),
            getLocalBounds().toFloat().translated((float) getX(), (float) getY()),
            scaleFactor);
}

std::vector<std::pair<String, Rectangle<float>>> GuideCurveEditorComponent::automationPointerTargets() const {
    const std::pair<const char*, const char*> targetIds[] {
            { "guideEditor.enabled", "guideEditor.enabled" },
            { "guideEditor.noise", "guideEditor.noise" },
            { "guideEditor.noise.value", "guideEditor.noise.value" },
            { "guideEditor.dcOffset", "guideEditor.dcOffset" },
            { "guideEditor.dcOffset.value", "guideEditor.dcOffset.value" },
            { "guideEditor.phase", "guideEditor.phase" },
            { "guideEditor.phase.value", "guideEditor.phase.value" },
            { "guideEditor.loadImage", "guideEditor.loadImage" },
            { "guideEditor.clearImage", "guideEditor.clearImage" },
            { "guideEditor.close", "curveEditor.close" }
    };
    std::vector<std::pair<String, Rectangle<float>>> result;
    for (const auto& [semanticId, componentId] : targetIds) {
        const Component* target = findChildWithID(componentId);
        if (target != nullptr) {
            result.emplace_back(
                    semanticId,
                    getLocalArea(target, target->getLocalBounds()).toFloat());
        }
    }
    return result;
}

Rectangle<float> GuideCurveEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromRight(kControlRailWidth);
}

Rectangle<float> GuideCurveEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromRight(kControlRailWidth);
    return bounds;
}

void GuideCurveEditorComponent::paintEditor(Graphics&) {
}

void GuideCurveEditorComponent::layoutEditor() {
    Rectangle<int> bounds = editorControlBounds().toNearestInt().reduced(12, 12);
    for (auto* slider : { &impl->noise, &impl->dcOffset, &impl->phase }) {
        slider->setBounds(
                bounds.removeFromTop(PropertyControlMetrics::rowHeight),
                PropertyControlMetrics::labelWidth,
                kSliderValueGap,
                kSliderValueWidth);
        bounds.removeFromTop(PropertyControlMetrics::rowGap);
    }
    bounds.removeFromTop(PropertyControlMetrics::sectionGap);
    impl->imageTitle.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(PropertyControlMetrics::rowGap);
    auto actionRow = bounds.removeFromTop(PropertyControlMetrics::rowHeight);
    actionRow = actionRow.withTrimmedTop(
            (PropertyControlMetrics::rowHeight - kActionButtonHeight) / 2);
    impl->loadButton.setBounds(
            actionRow.removeFromLeft(kActionButtonWidth).withHeight(kActionButtonHeight));
    actionRow.removeFromLeft(PropertyControlMetrics::inlineGap);
    impl->clearButton.setBounds(
            actionRow.removeFromLeft(kActionButtonWidth).withHeight(kActionButtonHeight));
}

void GuideCurveEditorComponent::syncEditorFromNode() {
    impl->enabled.setToggleState(guide.enabled, dontSendNotification);
    impl->noise.slider.setValue(guide.noise, dontSendNotification);
    impl->dcOffset.slider.setValue(guide.dcOffset, dontSendNotification);
    impl->phase.slider.setValue(guide.phase, dontSendNotification);
    impl->noise.refreshValueText();
    impl->dcOffset.refreshValueText();
    impl->phase.refreshValueText();
    updateHeatmapControls();
}

void GuideCurveEditorComponent::updateHeatmapControls() {
    impl->loadButton.setButtonText(heatmap != nullptr ? "Replace" : "Load");
    impl->loadButton.setEnabled(true);
    impl->clearButton.setEnabled(heatmap != nullptr);
}

void GuideCurveEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(
            impl->enabled.getToggleState(),
            static_cast<float>(impl->noise.slider.getValue()),
            static_cast<float>(impl->dcOffset.slider.getValue()),
            static_cast<float>(impl->phase.slider.getValue()),
            0);
}

std::vector<NodeParameter> GuideCurveEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    result.push_back({ "enabled", "Enabled", impl->enabled.getToggleState() ? "1" : "0" });
    result.push_back({ "noise", "Noise", String(impl->noise.slider.getValue(), 8) });
    result.push_back({ "dcOffset", "DC Offset", String(impl->dcOffset.slider.getValue(), 8) });
    result.push_back({ "phase", "Phase", String(impl->phase.slider.getValue(), 8) });
    return result;
}

void GuideCurveEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.getToggleState());
    state.setProperty("noise", impl->noise.slider.getValue());
    state.setProperty("dcOffset", impl->dcOffset.slider.getValue());
    state.setProperty("phase", impl->phase.slider.getValue());
    state.setProperty("noiseLayout", propertySliderRowAutomationState(impl->noise));
    state.setProperty("dcOffsetLayout", propertySliderRowAutomationState(impl->dcOffset));
    state.setProperty("phaseLayout", propertySliderRowAutomationState(impl->phase));
    state.setProperty("heatmapActive", heatmap != nullptr);
    state.setProperty("heatmapFilename", heatmap != nullptr ? heatmap->filename() : String {});
    state.setProperty("heatmapSectionLabel", impl->imageTitle.getText());
    state.setProperty("heatmapSublabelVisible", false);
    state.setProperty("heatmapLoadBounds", editorBoundsToVar(
            impl->loadButton.getBounds().toFloat()));
    state.setProperty("heatmapClearBounds", editorBoundsToVar(
            impl->clearButton.getBounds().toFloat()));
}

}

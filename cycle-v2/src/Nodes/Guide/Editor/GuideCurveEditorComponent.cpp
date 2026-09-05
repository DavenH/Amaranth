#include "Nodes/Guide/Editor/GuideCurveEditorComponent.h"

#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Guide/GuideHeatmapLoader.h"

#include "Graph/NodeParameterMap.h"

namespace CycleV2 {

namespace {

constexpr float kControlRailWidth = 236.f;
constexpr float kMaximumHostHeight = 560.f;

void showCompactValue(Slider& slider) {
    slider.setTextBoxStyle(Slider::TextBoxRight, false, 42, 22);
    slider.setNumDecimalPlacesToDisplay(2);
    slider.setColour(Slider::textBoxTextColourId, Colour(0xffaeb8c5));
    slider.setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour(Slider::textBoxHighlightColourId, Colour(0xff354659));
}

}

struct GuideCurveEditorComponent::Impl {
    explicit Impl(Component& owner) :
            enabled     (owner, "Enabled")
        ,   noise       (owner, "Noise")
        ,   dcOffset    (owner, "DC Offset")
        ,   phase       (owner, "Phase") {
        loadButton.setName("Load Guide heatmap");
        loadButton.setButtonText("Load image...");
        clearButton.setName("Clear Guide heatmap");
        clearButton.setButtonText("Clear image");
        status.setName("Guide heatmap status");
        status.setColour(Label::textColourId, Colour(0xffaeb8c5));
        status.setFont(FontOptions(12.f));
        status.setJustificationType(Justification::centredLeft);
        owner.addAndMakeVisible(loadButton);
        owner.addAndMakeVisible(clearButton);
        owner.addAndMakeVisible(status);
    }

    ParameterToggle enabled;
    LabeledParameterSlider noise;
    LabeledParameterSlider dcOffset;
    LabeledParameterSlider phase;
    TextButton loadButton;
    TextButton clearButton;
    Label status;
    std::unique_ptr<FileChooser> chooser;
    GuideHeatmapLoader loader;
};

GuideCurveEditorComponent::GuideCurveEditorComponent(CurveEditorWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    for (Slider* slider : {
            &impl->noise.slider,
            &impl->dcOffset.slider,
            &impl->phase.slider }) {
        slider->setRange(0.0, 1.0, 0.00001);
        showCompactValue(*slider);
    }
    bindDiscreteControl(impl->enabled);
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
                    safeThis->impl->status.setText("Loading...", dontSendNotification);
                    auto completion = [safeThis](GuideHeatmapAssetPtr asset, String error) mutable {
                        if (safeThis == nullptr) {
                            return;
                        }
                        safeThis->impl->loadButton.setEnabled(true);
                        if (asset == nullptr) {
                            safeThis->impl->status.setText(error, dontSendNotification);
                            return;
                        }
                        if (safeThis->loadHeatmap == nullptr
                                || !safeThis->loadHeatmap(
                                        safeThis->guide.id,
                                        asset,
                                        safeThis->guide.revision)) {
                            safeThis->impl->status.setText(
                                    "The Guide changed while the image was loading",
                                    dontSendNotification);
                        }
                    };
                    safeThis->impl->loader.load(file, std::move(completion));
                });
    };
    impl->clearButton.onClick = [this] {
        if (clearHeatmap != nullptr && clearHeatmap(guide.id, guide.revision)) {
            impl->status.setText("No image loaded", dontSendNotification);
        }
    };
}

GuideCurveEditorComponent::~GuideCurveEditorComponent() = default;

Rectangle<float> GuideCurveEditorComponent::preferredHostBounds(Rectangle<float> canvasBounds) {
    Rectangle<float> available = canvasBounds.reduced(36.f, 24.f);
    return available.withHeight(jmin(available.getHeight(), kMaximumHostHeight))
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
    Rectangle<int> bounds = editorControlBounds().toNearestInt().reduced(16, 18);
    constexpr int labelWidth = 70;
    constexpr int labelGap = 10;
    constexpr int rowHeight = 30;
    constexpr int rowGap = 10;

    impl->enabled.setBounds(bounds.removeFromTop(rowHeight), labelWidth, labelGap);
    bounds.removeFromTop(rowGap);
    for (auto* slider : { &impl->noise, &impl->dcOffset, &impl->phase }) {
        slider->setBounds(bounds.removeFromTop(rowHeight), labelWidth, labelGap);
        bounds.removeFromTop(rowGap);
    }
    bounds.removeFromTop(4);
    impl->loadButton.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(rowGap);
    impl->clearButton.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(4);
    impl->status.setBounds(bounds.removeFromTop(54));
}

void GuideCurveEditorComponent::syncEditorFromNode() {
    impl->enabled.button.setToggleState(guide.enabled, dontSendNotification);
    impl->noise.slider.setValue(guide.noise, dontSendNotification);
    impl->dcOffset.slider.setValue(guide.dcOffset, dontSendNotification);
    impl->phase.slider.setValue(guide.phase, dontSendNotification);
    impl->clearButton.setEnabled(heatmap != nullptr);
    const String status = heatmap != nullptr
            ? heatmap->filename() + " - " + String(heatmap->width())
                    + "x" + String(heatmap->height())
            : "No image loaded";
    impl->status.setText(status, dontSendNotification);
}

void GuideCurveEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(
            impl->enabled.button.getToggleState(),
            static_cast<float>(impl->noise.slider.getValue()),
            static_cast<float>(impl->dcOffset.slider.getValue()),
            static_cast<float>(impl->phase.slider.getValue()),
            0);
}

std::vector<NodeParameter> GuideCurveEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    result.push_back({ "enabled", "Enabled", impl->enabled.button.getToggleState() ? "1" : "0" });
    result.push_back({ "noise", "Noise", String(impl->noise.slider.getValue()) });
    result.push_back({ "dcOffset", "DC Offset", String(impl->dcOffset.slider.getValue()) });
    result.push_back({ "phase", "Phase", String(impl->phase.slider.getValue()) });
    return result;
}

void GuideCurveEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.button.getToggleState());
    state.setProperty("noise", impl->noise.slider.getValue());
    state.setProperty("dcOffset", impl->dcOffset.slider.getValue());
    state.setProperty("phase", impl->phase.slider.getValue());
    state.setProperty("heatmapActive", heatmap != nullptr);
    state.setProperty("heatmapFilename", heatmap != nullptr ? heatmap->filename() : String {});
    state.setProperty("heatmapStatus", impl->status.getText());
}

}

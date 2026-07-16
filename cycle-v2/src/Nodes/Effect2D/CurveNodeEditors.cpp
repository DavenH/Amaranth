#include "CurveNodeEditors.h"

#include "CurveNodeModels.h"
#include "../Trimesh/TrimeshSidePanelRenderer.h"

#include <array>
#include <utility>

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kEnvelopeControlsHeight = 246.f;

class RailLookAndFeel : public LookAndFeel_V4 {
public:
    void drawLinearSlider(Graphics& graphics, int x, int y, int width, int height,
            float sliderPosition, float, float, Slider::SliderStyle, Slider&) override {
        auto row = Rectangle<float>((float) x, (float) y, (float) width, (float) height);
        auto rail = row.withSizeKeepingCentre(row.getWidth(), 7.f);
        const float knobX = jlimit(rail.getX(), rail.getRight(), sliderPosition);
        graphics.setColour(Colour(0xff384351));
        graphics.fillRoundedRectangle(rail, 3.5f);
        graphics.setColour(Colour(0xffdce3ec).withAlpha(0.78f));
        graphics.fillRoundedRectangle(rail.withRight(knobX), 3.5f);
        graphics.setColour(Colour(0xff0d1116));
        graphics.fillEllipse(Rectangle<float>(16.f, 16.f).withCentre({ knobX, rail.getCentreY() }));
        graphics.setColour(Colour(0xffdce3ec));
        graphics.drawEllipse(Rectangle<float>(16.f, 16.f).withCentre({ knobX, rail.getCentreY() }), 1.5f);
    }
};

RailLookAndFeel& railLookAndFeel() {
    static RailLookAndFeel result;
    return result;
}

void styleLabel(Label& label, const String& text) {
    label.setText(text, dontSendNotification);
    label.setColour(Label::textColourId, kMutedText);
    label.setFont(FontOptions(12.f, Font::bold));
    label.setJustificationType(Justification::centredRight);
}

void styleSlider(Slider& slider) {
    slider.setSliderStyle(Slider::LinearHorizontal);
    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    slider.setRange(0.0, 1.0, 0.001);
    slider.setLookAndFeel(&railLookAndFeel());
}

void styleButton(TextButton& button, const String& text) {
    button.setButtonText(text);
    button.setColour(TextButton::buttonColourId, Colour(0xff161d25));
    button.setColour(TextButton::buttonOnColourId, Colour(0xff252f3b));
    button.setColour(TextButton::textColourOffId, kText);
    button.setColour(TextButton::textColourOnId, kText);
}

void addParameter(std::vector<NodeParameter>& result, const Node& node,
        const String& id, const String& name, const String& value) {
    String resolvedName = name;
    for (const auto& parameter : node.parameters) {
        if (parameter.id == id && parameter.label.isNotEmpty()) {
            resolvedName = parameter.label;
            break;
        }
    }
    result.push_back({ id, resolvedName, value });
}

String retainedParameter(const Node& node, const String& id, const String& fallback) {
    return parameterValueForNode(node, id, fallback);
}

void addCommonFlatControls(Component& owner, ToggleButton& enabled,
        std::initializer_list<Slider*> sliders, std::initializer_list<Label*> labels) {
    enabled.setButtonText("Enable");
    owner.addAndMakeVisible(enabled);
    for (auto* slider : sliders) {
        styleSlider(*slider);
        owner.addAndMakeVisible(*slider);
    }
    for (auto* label : labels) {
        owner.addAndMakeVisible(*label);
    }
}

void layoutRail(Rectangle<float> area, ToggleButton& enabled,
        std::initializer_list<std::pair<Label*, Slider*>> rows,
        std::initializer_list<TextButton*> buttons = {}) {
    Rectangle<int> bounds = area.toNearestInt().reduced(12, 8);
    constexpr int labelWidth = 82;
    constexpr int gap = 12;
    enabled.setBounds(bounds.removeFromTop(30).withTrimmedLeft(labelWidth + gap));
    bounds.removeFromTop(12);
    for (const auto& [label, slider] : rows) {
        auto row = bounds.removeFromTop(32);
        label->setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(gap);
        slider->setBounds(row.withTrimmedRight(4).withSizeKeepingCentre(row.getWidth() - 4, 26));
        bounds.removeFromTop(12);
    }
    for (auto* button : buttons) {
        auto row = bounds.removeFromTop(32);
        row.removeFromLeft(labelWidth + gap);
        button->setBounds(row.removeFromLeft(76).reduced(0, 2));
        bounds.removeFromTop(7);
    }
}

var boundsToVar(Rectangle<float> bounds) {
    auto* result = new DynamicObject();
    result->setProperty("x", bounds.getX());
    result->setProperty("y", bounds.getY());
    result->setProperty("width", bounds.getWidth());
    result->setProperty("height", bounds.getHeight());
    return result;
}

Rectangle<float> envelopeMorphSquareBounds(Rectangle<float> controls) {
    controls.reduce(12.f, 8.f);
    return controls.removeFromLeft(178.f);
}

Rectangle<float> envelopeMorphPlaneBounds(Rectangle<float> controls) {
    auto bounds = envelopeMorphSquareBounds(controls);
    bounds.removeFromTop(23.f);
    const float size = jmin(bounds.getWidth() - 20.f, bounds.getHeight() - 8.f);
    return Rectangle<float>(size, size).withCentre(bounds.getCentre());
}

Rectangle<float> envelopeRailColumn(Rectangle<float> controls) {
    controls.reduce(12.f, 8.f);
    controls.removeFromLeft(200.f);
    return controls.removeFromLeft(328.f);
}

Rectangle<float> envelopeMorphRow(Rectangle<float> controls, int axis) {
    auto row = envelopeRailColumn(controls);
    row.removeFromTop(41.f + 35.f * (float) axis);
    return row.removeFromTop(32.f);
}

Rectangle<float> envelopeAxisBounds(Rectangle<float> controls, int axis) {
    const auto row = envelopeMorphRow(controls, axis);
    return { row.getRight() - 52.f, row.getCentreY() - 10.f, 20.f, 20.f };
}

Rectangle<float> envelopeLinkBounds(Rectangle<float> controls, int axis) {
    const auto row = envelopeMorphRow(controls, axis);
    return { row.getRight() - 26.f, row.getCentreY() - 10.f, 20.f, 20.f };
}

Rectangle<float> envelopeVertexBounds(Rectangle<float> controls) {
    controls.reduce(12.f, 8.f);
    controls.removeFromLeft(546.f);
    return controls;
}

Colour axisColour(int axis) {
    static const Colour colours[] { Colour(0xffd7bf5f), Colour(0xffd65a5a), Colour(0xff5f91e8) };
    return colours[jlimit(0, 2, axis)];
}

void drawMorphPlane(Graphics& graphics, Rectangle<float> controls, float red, float blue) {
    auto column = envelopeMorphSquareBounds(controls);
    auto header = column.removeFromTop(23.f);
    graphics.setColour(kMutedText);
    graphics.setFont(FontOptions(10.5f, Font::bold));
    graphics.drawText("morph plane", header, Justification::centred);
    auto square = envelopeMorphPlaneBounds(controls);
    graphics.setColour(Colour(0xff5f91e8).withAlpha(0.20f));
    graphics.fillRect(square);
    graphics.setGradientFill(ColourGradient(Colour(0x00d65a5a), square.getCentreX(), square.getY(),
            Colour(0x66d65a5a), square.getCentreX(), square.getBottom(), false));
    graphics.fillRect(square);
    graphics.setColour(kText.withAlpha(0.3f));
    graphics.drawRect(square, 1.f);
    Point<float> cursor { square.getX() + square.getWidth() * red,
            square.getBottom() - square.getHeight() * blue };
    graphics.setColour(Colours::black.withAlpha(0.75f));
    graphics.fillEllipse(Rectangle<float>(8.f, 8.f).withCentre(cursor));
    graphics.setColour(kText);
    graphics.drawEllipse(Rectangle<float>(8.f, 8.f).withCentre(cursor), 1.2f);
}

}

struct WaveshaperEditorComponent::Impl {
    ToggleButton enabled;
    Slider preGain;
    Slider postGain;
    Label preGainLabel;
    Label postGainLabel;
    Label enabledLabel;
    ComboBox oversampling;
    Label oversamplingLabel;
};

WaveshaperEditorComponent::WaveshaperEditorComponent(Effect2DWidget& target) :
        CurveExpandedEditorComponent(target), impl(std::make_unique<Impl>()) {
    addCommonFlatControls(*this, impl->enabled, { &impl->preGain, &impl->postGain },
            { &impl->preGainLabel, &impl->postGainLabel });
    styleLabel(impl->enabledLabel, "Enable");
    styleLabel(impl->preGainLabel, "Pre");
    styleLabel(impl->postGainLabel, "Post");
    styleLabel(impl->oversamplingLabel, "AA factor");
    addAndMakeVisible(impl->enabledLabel);
    addAndMakeVisible(impl->oversamplingLabel);
    addAndMakeVisible(impl->oversampling);
    for (int value : { 1, 2, 4, 8 }) {
        impl->oversampling.addItem(String(value), value);
    }
    auto publish = [this] { publishCurrentState(); requestRepaint(); };
    impl->enabled.onClick = publish;
    impl->preGain.onValueChange = publish;
    impl->postGain.onValueChange = publish;
    impl->oversampling.onChange = publish;
    for (Slider* slider : { &impl->preGain, &impl->postGain }) {
        slider->onDragStart = [this] { beginTransaction(); };
        slider->onDragEnd = [this] { commitTransaction(); };
    }
}
WaveshaperEditorComponent::~WaveshaperEditorComponent() = default;
Rectangle<float> WaveshaperEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromRight(224.f);
}
Rectangle<float> WaveshaperEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromRight(224.f);
    bounds.reduce(18.f, 14.f);
    const float size = jmin(300.f, jmin(bounds.getWidth(), bounds.getHeight()));
    return Rectangle<float>(size, size).withX(bounds.getX()).withCentre({ bounds.getX() + size * 0.5f, bounds.getCentreY() });
}
void WaveshaperEditorComponent::paintEditor(Graphics&) {}
void WaveshaperEditorComponent::layoutEditor() {
    auto bounds = editorControlBounds().toNearestInt().reduced(12, 8);
    bounds = bounds.withSizeKeepingCentre(jmin(bounds.getWidth(), 206), jmin(bounds.getHeight(), 175));
    auto place = [&](Label& label, Component& component, int height) {
        auto row = bounds.removeFromTop(34);
        label.setBounds(row.removeFromLeft(78));
        row.removeFromLeft(12);
        component.setBounds(row.removeFromLeft(116).withSizeKeepingCentre(116, height));
        bounds.removeFromTop(13);
    };
    place(impl->enabledLabel, impl->enabled, 26);
    place(impl->preGainLabel, impl->preGain, 26);
    place(impl->postGainLabel, impl->postGain, 26);
    place(impl->oversamplingLabel, impl->oversampling, 32);
}
void WaveshaperEditorComponent::syncEditorFromNode() {
    WaveshaperNodeModel model; model.syncFromNode(node);
    impl->enabled.setToggleState(model.enabled, dontSendNotification);
    impl->preGain.setValue(model.preGain, dontSendNotification);
    impl->postGain.setValue(model.postGain, dontSendNotification);
    impl->oversampling.setSelectedId(model.oversampling, dontSendNotification);
}
void WaveshaperEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(impl->enabled.getToggleState(), (float) impl->preGain.getValue(),
            (float) impl->postGain.getValue(), 0.5f, impl->oversampling.getSelectedId());
}
std::vector<NodeParameter> WaveshaperEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    addParameter(result, node, "enabled", "Enabled", impl->enabled.getToggleState() ? "1" : "0");
    addParameter(result, node, "pre", "Pre Gain", String(impl->preGain.getValue()));
    addParameter(result, node, "post", "Post Gain", String(impl->postGain.getValue()));
    addParameter(result, node, "aaFactor", "AA Factor", String(impl->oversampling.getSelectedId()));
    return result;
}
void WaveshaperEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.getToggleState());
    state.setProperty("preGain", impl->preGain.getValue());
    state.setProperty("postGain", impl->postGain.getValue());
    state.setProperty("oversampling", impl->oversampling.getSelectedId());
}

struct ImpulseResponseEditorComponent::Impl {
    ToggleButton enabled; Slider size; Slider postGain; Slider highPass;
    Label sizeLabel; Label postGainLabel; Label highPassLabel;
    TextButton load; TextButton unload; TextButton model;
};
ImpulseResponseEditorComponent::ImpulseResponseEditorComponent(Effect2DWidget& target) :
        CurveExpandedEditorComponent(target), impl(std::make_unique<Impl>()) {
    addCommonFlatControls(*this, impl->enabled, { &impl->size, &impl->postGain, &impl->highPass },
            { &impl->sizeLabel, &impl->postGainLabel, &impl->highPassLabel });
    styleLabel(impl->sizeLabel, "Size"); styleLabel(impl->postGainLabel, "Post");
    styleLabel(impl->highPassLabel, "HighPass");
    for (auto* button : { &impl->load, &impl->unload, &impl->model }) { addAndMakeVisible(*button); }
    styleButton(impl->load, "Load"); styleButton(impl->unload, "Unload"); styleButton(impl->model, "Model");
    auto publish = [this] { publishCurrentState(); requestRepaint(); };
    impl->enabled.onClick = publish;
    for (Slider* slider : { &impl->size, &impl->postGain, &impl->highPass }) {
        slider->onValueChange = publish;
        slider->onDragStart = [this] { beginTransaction(); };
        slider->onDragEnd = [this] { commitTransaction(); };
    }
}
ImpulseResponseEditorComponent::~ImpulseResponseEditorComponent() = default;
Rectangle<float> ImpulseResponseEditorComponent::editorControlBounds() const { auto b = contentBounds(); return b.removeFromRight(212.f); }
Rectangle<float> ImpulseResponseEditorComponent::editorPanelBounds() const { auto b = contentBounds(); b.removeFromRight(212.f); return b; }
void ImpulseResponseEditorComponent::paintEditor(Graphics&) {}
void ImpulseResponseEditorComponent::layoutEditor() { layoutRail(editorControlBounds(), impl->enabled,
        { { &impl->sizeLabel, &impl->size }, { &impl->postGainLabel, &impl->postGain }, { &impl->highPassLabel, &impl->highPass } },
        { &impl->load, &impl->unload, &impl->model }); }
void ImpulseResponseEditorComponent::syncEditorFromNode() {
    ImpulseResponseNodeModel model; model.syncFromNode(node);
    impl->enabled.setToggleState(model.enabled, dontSendNotification); impl->size.setValue(model.size, dontSendNotification);
    impl->postGain.setValue(model.postGain, dontSendNotification); impl->highPass.setValue(model.highPass, dontSendNotification);
}
void ImpulseResponseEditorComponent::applyEditorStateToWidget() { widget.setControlValues(impl->enabled.getToggleState(),
        (float) impl->size.getValue(), (float) impl->postGain.getValue(), (float) impl->highPass.getValue(), 0); }
std::vector<NodeParameter> ImpulseResponseEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    addParameter(result, node, "enabled", "Enabled", impl->enabled.getToggleState() ? "1" : "0");
    addParameter(result, node, "size", "Size", String(impl->size.getValue()));
    addParameter(result, node, "post", "Post Gain", String(impl->postGain.getValue()));
    addParameter(result, node, "highPass", "High Pass", String(impl->highPass.getValue()));
    return result;
}
void ImpulseResponseEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.getToggleState()); state.setProperty("size", impl->size.getValue());
    state.setProperty("postGain", impl->postGain.getValue()); state.setProperty("highPass", impl->highPass.getValue());
}

struct GuideCurveEditorComponent::Impl {
    ToggleButton enabled; Slider noise; Slider dcOffset; Slider phase;
    Label noiseLabel; Label dcOffsetLabel; Label phaseLabel; TextButton add; TextButton remove;
};
GuideCurveEditorComponent::GuideCurveEditorComponent(Effect2DWidget& target) :
        CurveExpandedEditorComponent(target), impl(std::make_unique<Impl>()) {
    addCommonFlatControls(*this, impl->enabled, { &impl->noise, &impl->dcOffset, &impl->phase },
            { &impl->noiseLabel, &impl->dcOffsetLabel, &impl->phaseLabel });
    styleLabel(impl->noiseLabel, "Noise"); styleLabel(impl->dcOffsetLabel, "DC Offset"); styleLabel(impl->phaseLabel, "Phase");
    addAndMakeVisible(impl->add); addAndMakeVisible(impl->remove); styleButton(impl->add, "+"); styleButton(impl->remove, "-");
    auto publish = [this] { publishCurrentState(); requestRepaint(); };
    impl->enabled.onClick = publish;
    for (Slider* slider : { &impl->noise, &impl->dcOffset, &impl->phase }) {
        slider->onValueChange = publish; slider->onDragStart = [this] { beginTransaction(); };
        slider->onDragEnd = [this] { commitTransaction(); };
    }
}
GuideCurveEditorComponent::~GuideCurveEditorComponent() = default;
Rectangle<float> GuideCurveEditorComponent::editorControlBounds() const { auto b = contentBounds(); return b.removeFromRight(196.f); }
Rectangle<float> GuideCurveEditorComponent::editorPanelBounds() const { auto b = contentBounds(); b.removeFromRight(196.f); return b; }
void GuideCurveEditorComponent::paintEditor(Graphics&) {}
void GuideCurveEditorComponent::layoutEditor() { layoutRail(editorControlBounds(), impl->enabled,
        { { &impl->noiseLabel, &impl->noise }, { &impl->dcOffsetLabel, &impl->dcOffset }, { &impl->phaseLabel, &impl->phase } },
        { &impl->add, &impl->remove }); }
void GuideCurveEditorComponent::syncEditorFromNode() {
    GuideCurveNodeModel model; model.syncFromNode(node); impl->enabled.setToggleState(model.enabled, dontSendNotification);
    impl->noise.setValue(model.noise, dontSendNotification); impl->dcOffset.setValue(model.dcOffset, dontSendNotification);
    impl->phase.setValue(model.phase, dontSendNotification);
}
void GuideCurveEditorComponent::applyEditorStateToWidget() { widget.setControlValues(impl->enabled.getToggleState(),
        (float) impl->noise.getValue(), (float) impl->dcOffset.getValue(), (float) impl->phase.getValue(), 0); }
std::vector<NodeParameter> GuideCurveEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    addParameter(result, node, "enabled", "Enabled", impl->enabled.getToggleState() ? "1" : "0");
    addParameter(result, node, "noise", "Noise", String(impl->noise.getValue()));
    addParameter(result, node, "dcOffset", "DC Offset", String(impl->dcOffset.getValue()));
    addParameter(result, node, "phase", "Phase", String(impl->phase.getValue())); return result;
}
void GuideCurveEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.getToggleState()); state.setProperty("noise", impl->noise.getValue());
    state.setProperty("dcOffset", impl->dcOffset.getValue()); state.setProperty("phase", impl->phase.getValue());
}

struct EnvelopeEditorComponent::Impl {
    Slider redMorph; Slider blueMorph; Label timeLabel; Label redLabel; Label blueLabel;
    TextButton loop { "Loop" }; TextButton sustain { "Sustain" }; TextButton logarithmic { "Log" };
    TextButton dynamic { "Live" };
    int viewAxis {}; bool redLinked { true }; bool blueLinked { true };
    bool draggingMorph {}; bool draggingParameter {}; String parameterId;
};
EnvelopeEditorComponent::EnvelopeEditorComponent(Effect2DWidget& target) :
        CurveExpandedEditorComponent(target), impl(std::make_unique<Impl>()) {
    for (auto* slider : { &impl->redMorph, &impl->blueMorph }) { styleSlider(*slider); addAndMakeVisible(*slider); }
    styleLabel(impl->timeLabel, "Time"); styleLabel(impl->redLabel, "Red"); styleLabel(impl->blueLabel, "Blue");
    for (auto* label : { &impl->timeLabel, &impl->redLabel, &impl->blueLabel }) { addAndMakeVisible(*label); }
    for (auto* button : { &impl->loop, &impl->sustain, &impl->logarithmic, &impl->dynamic }) { styleButton(*button, button->getButtonText()); addAndMakeVisible(*button); }
    impl->logarithmic.setClickingTogglesState(true);
    impl->dynamic.setClickingTogglesState(true);
    auto morph = [this] { publishCurrentState(); requestRepaint(); };
    impl->redMorph.onValueChange = morph; impl->blueMorph.onValueChange = morph;
    for (auto* slider : { &impl->redMorph, &impl->blueMorph }) {
        slider->onDragStart = [this] { beginTransaction(); }; slider->onDragEnd = [this] { commitTransaction(); };
    }
    impl->loop.onClick = [this] { beginTransaction(); widget.toggleSelectedEnvelopeMarker(true); publishCurrentState(); commitTransaction(); requestRepaint(); };
    impl->sustain.onClick = [this] { beginTransaction(); widget.toggleSelectedEnvelopeMarker(false); publishCurrentState(); commitTransaction(); requestRepaint(); };
    impl->logarithmic.onClick = [this] { beginTransaction(); widget.setEnvelopeLogarithmic(impl->logarithmic.getToggleState()); publishCurrentState(); commitTransaction(); requestRepaint(); };
    impl->dynamic.onClick = [this] { beginTransaction(); publishCurrentState(); commitTransaction(); requestRepaint(); };
}
EnvelopeEditorComponent::~EnvelopeEditorComponent() = default;
Rectangle<float> EnvelopeEditorComponent::editorControlBounds() const { auto b = contentBounds(); return b.removeFromTop(kEnvelopeControlsHeight); }
Rectangle<float> EnvelopeEditorComponent::editorPanelBounds() const { auto b = contentBounds(); b.removeFromTop(kEnvelopeControlsHeight); return b; }
void EnvelopeEditorComponent::paintEditor(Graphics& graphics) {
    const auto controls = editorControlBounds();
    drawMorphPlane(graphics, controls, (float) impl->redMorph.getValue(), (float) impl->blueMorph.getValue());
    const bool linked[] { true, impl->redLinked, impl->blueLinked };
    for (int axis = 0; axis < 3; ++axis) {
        auto axisBounds = envelopeAxisBounds(controls, axis); auto linkBounds = envelopeLinkBounds(controls, axis);
        const auto colour = axisColour(axis);
        graphics.setColour(colour.withAlpha(axis == impl->viewAxis ? 0.45f : 0.06f)); graphics.fillRoundedRectangle(axisBounds, 4.f);
        graphics.setColour(colour.withAlpha(axis == impl->viewAxis ? 1.f : 0.32f)); graphics.drawRoundedRectangle(axisBounds, 4.f, 1.4f);
        graphics.setColour(colour.withAlpha(linked[axis] ? 0.9f : 0.25f)); graphics.drawRoundedRectangle(linkBounds, 4.f, 1.4f);
    }
    std::array<String, 6> guides {};
    TrimeshSidePanelRenderer::drawVertexParameters(graphics, envelopeVertexBounds(controls), widget.selectedVertexParameters(), guides);
}
void EnvelopeEditorComponent::layoutEditor() {
    const auto controls = editorControlBounds();
    auto place = [&](Label& label, Slider& slider, int axis) {
        auto row = envelopeMorphRow(controls, axis).toNearestInt(); label.setBounds(row.removeFromLeft(42));
        row.removeFromRight(58); slider.setBounds(row);
    };
    impl->timeLabel.setBounds(envelopeMorphRow(controls, 0).withWidth(42.f).toNearestInt());
    place(impl->redLabel, impl->redMorph, 1); place(impl->blueLabel, impl->blueMorph, 2);
    auto markerRow = envelopeRailColumn(controls).toNearestInt(); markerRow.removeFromTop(160); markerRow = markerRow.removeFromTop(30);
    impl->loop.setBounds(markerRow.removeFromLeft(70).reduced(2)); markerRow.removeFromLeft(8);
    impl->sustain.setBounds(markerRow.removeFromLeft(70).reduced(2)); markerRow.removeFromLeft(8);
    impl->logarithmic.setBounds(markerRow.removeFromLeft(54).reduced(2)); markerRow.removeFromLeft(8);
    impl->dynamic.setBounds(markerRow.removeFromLeft(54).reduced(2));
}
void EnvelopeEditorComponent::syncEditorFromNode() {
    EnvelopeNodeModel model; model.syncFromNode(node);
    impl->redMorph.setValue(model.red, dontSendNotification); impl->blueMorph.setValue(model.blue, dontSendNotification);
    impl->redLinked = model.redLinked; impl->blueLinked = model.blueLinked;
    impl->logarithmic.setToggleState(model.logarithmic, dontSendNotification);
    impl->dynamic.setToggleState(model.dynamicWhileLive, dontSendNotification);
    widget.setEnvelopeAxisLinks(impl->redLinked, impl->blueLinked); widget.setEnvelopeLogarithmic(model.logarithmic);
    impl->loop.setToggleState(widget.selectedEnvelopeMarkerState(true), dontSendNotification);
    impl->sustain.setToggleState(widget.selectedEnvelopeMarkerState(false), dontSendNotification);
}
void EnvelopeEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(true, (float) impl->redMorph.getValue(), (float) impl->blueMorph.getValue(), 0.5f, 0);
    widget.setEnvelopeAxisLinks(impl->redLinked, impl->blueLinked);
    widget.setEnvelopeLogarithmic(impl->logarithmic.getToggleState());
}
std::vector<NodeParameter> EnvelopeEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    addParameter(result, node, "logarithmic", "Logarithmic", impl->logarithmic.getToggleState() ? "1" : "0");
    addParameter(result, node, "dynamic", "Dynamic While Live", impl->dynamic.getToggleState() ? "1" : "0");
    addParameter(result, node, "red", "Red Morph", String(impl->redMorph.getValue()));
    addParameter(result, node, "blue", "Blue Morph", String(impl->blueMorph.getValue()));
    addParameter(result, node, "level", "Level", retainedParameter(node, "level", "1"));
    return result;
}
void EnvelopeEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("redMorph", impl->redMorph.getValue()); state.setProperty("blueMorph", impl->blueMorph.getValue());
    state.setProperty("viewAxis", impl->viewAxis); state.setProperty("logarithmic", impl->logarithmic.getToggleState());
    state.setProperty("dynamic", impl->dynamic.getToggleState());
    state.setProperty("morphPlaneBounds", boundsToVar(envelopeMorphPlaneBounds(editorControlBounds())));
}
bool EnvelopeEditorComponent::editorMouseMove(Point<float> position) {
    const auto controls = editorControlBounds();
    bool interactive = envelopeMorphPlaneBounds(controls).contains(position);
    for (int axis = 0; axis < 3; ++axis) {
        interactive = interactive || envelopeAxisBounds(controls, axis).contains(position)
                || (axis > 0 && envelopeLinkBounds(controls, axis).contains(position));
    }
    const auto parameters = widget.selectedVertexParameters();
    const auto parameterArea = envelopeVertexBounds(controls);
    for (int index = 0; index < (int) parameters.size(); ++index) {
        const auto row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, index);
        interactive = interactive
                || TrimeshSidePanelRenderer::vertexParameterRailBounds(row).expanded(5.f, 8.f).contains(position)
                || TrimeshSidePanelRenderer::vertexParameterGuideBounds(row).expanded(4.f).contains(position);
    }
    setMouseCursor(interactive ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
    return interactive;
}
bool EnvelopeEditorComponent::editorMouseDown(Point<float> position) {
    impl->draggingMorph = false; impl->draggingParameter = false; impl->parameterId.clear();
    const auto controls = editorControlBounds();
    for (int axis = 0; axis < 3; ++axis) {
        if (envelopeAxisBounds(controls, axis).contains(position)) { impl->viewAxis = axis; requestRepaint(); return true; }
        if (axis > 0 && envelopeLinkBounds(controls, axis).contains(position)) {
            (axis == 1 ? impl->redLinked : impl->blueLinked) = !(axis == 1 ? impl->redLinked : impl->blueLinked);
            publishCurrentState(); requestRepaint(); return true;
        }
    }
    const auto parameters = widget.selectedVertexParameters(); const auto parameterArea = envelopeVertexBounds(controls);
    for (int index = 0; index < (int) parameters.size(); ++index) {
        const auto row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, index);
        const auto guide = TrimeshSidePanelRenderer::vertexParameterGuideBounds(row);
        if (guide.expanded(4.f).contains(position)) {
            PopupMenu menu;
            menu.addItem(1, "Guide attachments are available on mesh nodes", false, false);
            menu.showMenuAsync(PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(guide.toNearestInt())));
            return true;
        }
        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(row);
        if (rail.expanded(5.f, 8.f).contains(position)) {
            impl->parameterId = parameters[(size_t) index].id; impl->draggingParameter = true;
            widget.setSelectedVertexParameter(impl->parameterId, jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth()));
            return true;
        }
    }
    const auto plane = envelopeMorphPlaneBounds(controls);
    if (plane.contains(position)) { impl->draggingMorph = true; return editorMouseDrag(position); }
    return false;
}
bool EnvelopeEditorComponent::editorMouseDrag(Point<float> position) {
    if (impl->draggingMorph) {
        const auto plane = envelopeMorphPlaneBounds(editorControlBounds());
        impl->redMorph.setValue(jlimit(0.f, 1.f, (position.x - plane.getX()) / plane.getWidth()), sendNotificationSync);
        impl->blueMorph.setValue(jlimit(0.f, 1.f, (plane.getBottom() - position.y) / plane.getHeight()), sendNotificationSync);
        return true;
    }
    if (impl->draggingParameter) {
        const auto parameters = widget.selectedVertexParameters(); const auto area = envelopeVertexBounds(editorControlBounds());
        for (int index = 0; index < (int) parameters.size(); ++index) {
            if (parameters[(size_t) index].id != impl->parameterId) { continue; }
            const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(
                    TrimeshSidePanelRenderer::vertexParameterRowBounds(area, index));
            if (widget.setSelectedVertexParameter(impl->parameterId,
                    jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth()))) { requestRepaint(); }
            return true;
        }
    }
    return false;
}
void EnvelopeEditorComponent::editorMouseUp() {
    impl->draggingMorph = false; impl->draggingParameter = false; impl->parameterId.clear();
}

std::unique_ptr<CurveExpandedEditorComponent> createCurveNodeEditor(NodeKind kind, Effect2DWidget& widget) {
    switch (kind) {
        case NodeKind::Waveshaper:      return std::make_unique<WaveshaperEditorComponent>(widget);
        case NodeKind::ImpulseResponse: return std::make_unique<ImpulseResponseEditorComponent>(widget);
        case NodeKind::GuideCurve:      return std::make_unique<GuideCurveEditorComponent>(widget);
        case NodeKind::Envelope:        return std::make_unique<EnvelopeEditorComponent>(widget);
        default:                        return {};
    }
}

}

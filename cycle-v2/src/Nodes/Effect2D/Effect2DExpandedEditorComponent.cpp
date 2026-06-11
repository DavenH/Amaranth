#include "Effect2DExpandedEditorComponent.h"

#include "../Trimesh/TrimeshSidePanelRenderer.h"

#include <utility>

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kHeaderHeight = 34.f;
constexpr int kGuideControlWidth = 126;
constexpr int kIrControlWidth = 144;
constexpr int kWaveshaperControlWidth = 150;
constexpr float kEnvelopeTopControlsHeight = 214.f;
constexpr float kEnvelopeAxisButtonSize = 20.f;
constexpr float kEnvelopeRowButtonGap = 6.f;
constexpr float kEnvelopeMorphSquareWidth = 178.f;
constexpr float kEnvelopeMorphGap = 22.f;
constexpr float kEnvelopeMorphRailWidth = 328.f;
constexpr float kEnvelopeVertexGap = 18.f;
constexpr float kEnvelopeSectionHeaderHeight = 18.f;
constexpr float kEnvelopeSectionHeaderTopInset = 5.f;

bool usesRightControlRail(NodeKind kind) {
    return kind == NodeKind::GuideCurve
            || kind == NodeKind::ImpulseResponse
            || kind == NodeKind::Waveshaper;
}

int rightControlRailWidth(NodeKind kind) {
    if (kind == NodeKind::ImpulseResponse) {
        return kIrControlWidth;
    }

    if (kind == NodeKind::Waveshaper) {
        return kWaveshaperControlWidth;
    }

    return kGuideControlWidth;
}

Colour envelopeAxisColour(const String& label) {
    if (label == "Red") {
        return Colour(0xffd65a5a);
    }

    if (label == "Blue") {
        return Colour(0xff5f91e8);
    }

    return Colour(0xffd8d5ca);
}

var rectangleToVar(Rectangle<float> bounds) {
    auto* object = new DynamicObject();
    object->setProperty("x", bounds.getX());
    object->setProperty("y", bounds.getY());
    object->setProperty("width", bounds.getWidth());
    object->setProperty("height", bounds.getHeight());
    return object;
}

class LegacyKnobLookAndFeel :
        public LookAndFeel_V4 {
public:
    void drawRotarySlider(
            Graphics& g,
            int x,
            int y,
            int width,
            int height,
            float sliderPos,
            float rotaryStartAngle,
            float rotaryEndAngle,
            Slider& slider) override {
        ignoreUnused(rotaryStartAngle, rotaryEndAngle, slider);

        const float size = (float) jmin(width, height);
        const float centreX = (float) x + (float) width * 0.5f;
        const float centreY = (float) y + (float) height * 0.5f;
        const float theta = 1.2f;
        const float radius = size * 0.36f;
        const float thickness = size * 0.15f;
        const float pi = MathConstants<float>::pi;
        const float startAngle = pi + theta * 0.5f;
        const float endAngle = 3.f * pi - theta * 0.5f;
        const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);

        Path band;
        band.addCentredArc(centreX, centreY, radius, radius, 0.f, startAngle, endAngle, true);

        Path value;
        value.addCentredArc(centreX, centreY, radius, radius, 0.f, startAngle, valueAngle, true);

        g.setColour(Colour(0xff485464));
        g.strokePath(band, PathStrokeType(thickness, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(Colour(0xffe6ebf2));
        g.strokePath(value, PathStrokeType(thickness, PathStrokeType::curved, PathStrokeType::rounded));
    }
};

LegacyKnobLookAndFeel& legacyKnobLookAndFeel() {
    static LegacyKnobLookAndFeel lookAndFeel;
    return lookAndFeel;
}

class MorphRailLookAndFeel :
        public LookAndFeel_V4 {
public:
    void drawLinearSlider(
            Graphics& g,
            int x,
            int y,
            int width,
            int height,
            float sliderPos,
            float minSliderPos,
            float maxSliderPos,
            const Slider::SliderStyle style,
            Slider& slider) override {
        ignoreUnused(minSliderPos, maxSliderPos, style);

        auto row = Rectangle<float>((float) x, (float) y, (float) width, (float) height);
        auto body = row.withTrimmedTop(4.f).withTrimmedBottom(4.f);
        auto rail = body.reduced(10.f, body.getHeight() * 0.5f - 1.f);
        const Colour colour = slider.findColour(Slider::thumbColourId);
        const float knobX = jlimit(rail.getX(), rail.getRight(), sliderPos);

        g.setColour(Colour(0xff050607).withAlpha(0.86f));
        g.fillRect(body);

        g.setColour(Colour(0xff59606a).withAlpha(0.16f));
        for (float scanY = body.getY() + 2.f; scanY < body.getBottom(); scanY += 3.f) {
            g.drawHorizontalLine(roundToInt(scanY), body.getX(), body.getRight());
        }

        g.setColour(Colour(0xffd8d5ca).withAlpha(0.22f));
        g.drawRect(body, 1.f);

        g.setColour(Colour(0xff2c3441));
        g.fillRoundedRectangle(rail, 1.f);

        for (int i = 0; i < 3; ++i) {
            const float xPos = rail.getX() + rail.getWidth() * (float) i * 0.5f;
            g.setColour(Colour(0xffd8d5ca).withAlpha(0.25f));
            g.drawVerticalLine(roundToInt(xPos), rail.getY() - 4.f, rail.getBottom() + 4.f);
        }

        g.setColour(colour.withAlpha(0.18f));
        g.fillRect(rail.withWidth(jmax(0.f, knobX - rail.getX())).expanded(0.f, 8.f));

        g.setColour(colour.withAlpha(0.76f));
        g.fillRoundedRectangle(rail.withWidth(jmax(0.f, knobX - rail.getX())), 1.f);

        g.setColour(Colour(0xff202328).withAlpha(0.88f));
        g.fillEllipse(Rectangle<float>(8.f, 8.f).withCentre({ knobX, rail.getCentreY() }));
        g.setColour(colour.withAlpha(0.88f));
        g.drawEllipse(Rectangle<float>(8.f, 8.f).withCentre({ knobX, rail.getCentreY() }), 1.4f);
        g.setColour(Colour(0xff05070a).withAlpha(0.72f));
        g.drawVerticalLine(roundToInt(knobX), rail.getY() - 5.f, rail.getBottom() + 5.f);
    }
};

MorphRailLookAndFeel& morphRailLookAndFeel() {
    static MorphRailLookAndFeel lookAndFeel;
    return lookAndFeel;
}

Rectangle<float> envelopeMorphSquareBounds(Rectangle<float> controls) {
    controls.reduce(12.f, 8.f);
    return controls.removeFromLeft(kEnvelopeMorphSquareWidth);
}

Rectangle<float> envelopeMorphPlaneBounds(Rectangle<float> controls) {
    Rectangle<float> bounds = envelopeMorphSquareBounds(controls);
    bounds.removeFromTop(kEnvelopeSectionHeaderTopInset + kEnvelopeSectionHeaderHeight);
    const float size = jmin(bounds.getWidth() - 20.f, bounds.getHeight() - 8.f);
    return Rectangle<float>(size, size).withCentre(bounds.getCentre());
}

Rectangle<float> envelopeMorphRailColumnBounds(Rectangle<float> controls) {
    controls.reduce(12.f, 8.f);
    controls.removeFromLeft(kEnvelopeMorphSquareWidth + kEnvelopeMorphGap);
    return controls.removeFromLeft(kEnvelopeMorphRailWidth);
}

Rectangle<float> envelopeMorphRowBounds(Rectangle<float> controls, int axisIndex) {
    Rectangle<float> column = envelopeMorphRailColumnBounds(controls);
    column.removeFromTop(kEnvelopeSectionHeaderTopInset + kEnvelopeSectionHeaderHeight);
    column.removeFromTop(18.f);
    column.translate(0.f, (float) axisIndex * 43.f);
    return column.removeFromTop(35.f);
}

Rectangle<float> envelopeMorphRailBounds(Rectangle<float> controls, int axisIndex) {
    Rectangle<float> row = envelopeMorphRowBounds(controls, axisIndex);
    row.removeFromLeft(42.f);
    row.removeFromRight(kEnvelopeAxisButtonSize * 2.f + kEnvelopeRowButtonGap * 3.f);
    return row;
}

Rectangle<float> envelopePrimaryAxisBounds(Rectangle<float> controls, int axisIndex) {
    const Rectangle<float> row = envelopeMorphRowBounds(controls, axisIndex);
    return {
            row.getRight() - kEnvelopeAxisButtonSize * 2.f - kEnvelopeRowButtonGap * 2.f,
            row.getCentreY() - kEnvelopeAxisButtonSize * 0.5f,
            kEnvelopeAxisButtonSize,
            kEnvelopeAxisButtonSize
    };
}

Rectangle<float> envelopeLinkToggleBounds(Rectangle<float> controls, int axisIndex) {
    const Rectangle<float> row = envelopeMorphRowBounds(controls, axisIndex);
    return {
            row.getRight() - kEnvelopeAxisButtonSize - kEnvelopeRowButtonGap,
            row.getCentreY() - kEnvelopeAxisButtonSize * 0.5f,
            kEnvelopeAxisButtonSize,
            kEnvelopeAxisButtonSize
    };
}

Rectangle<float> envelopeVertexParameterBounds(Rectangle<float> controls) {
    controls.reduce(12.f, 8.f);
    controls.removeFromLeft(
            kEnvelopeMorphSquareWidth
                    + kEnvelopeMorphGap
                    + kEnvelopeMorphRailWidth
                    + kEnvelopeVertexGap);
    return controls;
}

void drawEnvelopeMorphSquare(Graphics& g, Rectangle<float> bounds, float red, float blue) {
    if (bounds.isEmpty()) {
        return;
    }

    bounds.removeFromTop(kEnvelopeSectionHeaderTopInset);
    auto header = bounds.removeFromTop(kEnvelopeSectionHeaderHeight);
    g.setColour(kMutedText);
    g.setFont(FontOptions(10.5f, Font::bold));
    g.drawText("morph plane", header, Justification::centred);

    const float size = jmin(bounds.getWidth() - 20.f, bounds.getHeight() - 8.f);
    auto square = Rectangle<float>(size, size).withCentre(bounds.getCentre());
    const Colour redColour(0xffd65a5a);
    const Colour blueColour(0xff5f91e8);

    g.setGradientFill(ColourGradient(
            redColour.withAlpha(0.38f),
            square.getRight(),
            square.getY(),
            blueColour.withAlpha(0.38f),
            square.getX(),
            square.getBottom(),
            false));
    g.fillRect(square);

    g.setColour(Colour(0xff050607).withAlpha(0.38f));
    g.fillRect(square);
    g.setColour(Colour(0xffd8d5ca).withAlpha(0.28f));
    g.drawRect(square, 1.f);

    const float axisBarOffset = 4.f;
    g.setColour(redColour.withAlpha(0.72f));
    g.drawLine(
            square.getX(),
            square.getBottom() + axisBarOffset,
            square.getRight(),
            square.getBottom() + axisBarOffset,
            2.f);
    g.setColour(blueColour.withAlpha(0.72f));
    g.drawLine(
            square.getX() - axisBarOffset,
            square.getBottom(),
            square.getX() - axisBarOffset,
            square.getY(),
            2.f);

    const Point<float> cursor {
            square.getX() + square.getWidth() * jlimit(0.f, 1.f, red),
            square.getBottom() - square.getHeight() * jlimit(0.f, 1.f, blue)
    };

    g.setColour(Colours::black.withAlpha(0.72f));
    g.fillEllipse(Rectangle<float>(8.f, 8.f).withCentre(cursor));
    g.setColour(kText);
    g.drawEllipse(Rectangle<float>(8.f, 8.f).withCentre(cursor), 1.2f);
}

void drawEnvelopeMorphButtonRow(Graphics& g, Rectangle<float> controls) {
    const Colour colours[2] {
            Colour(0xffd65a5a),
            Colour(0xff5f91e8)
    };

    const Rectangle<float> axisHeader = envelopePrimaryAxisBounds(controls, 0);
    const Rectangle<float> linkHeader = envelopeLinkToggleBounds(controls, 0);
    const float headerY = envelopeMorphRowBounds(controls, 0).getY() - 13.f;

    g.setColour(kMutedText.withAlpha(0.78f));
    g.setFont(FontOptions(8.5f, Font::bold));
    g.drawText("axis", axisHeader.withY(headerY).withHeight(10.f), Justification::centred);
    g.drawText("link", linkHeader.withY(headerY).withHeight(10.f), Justification::centred);

    for (int i = 0; i < 2; ++i) {
        const bool primary = i == 0;
        const bool linked = false;
        const Colour colour = colours[i];
        const Rectangle<float> axis = envelopePrimaryAxisBounds(controls, i);
        const Rectangle<float> link = envelopeLinkToggleBounds(controls, i);

        g.setColour(colour.withAlpha(primary ? 0.42f : 0.055f));
        g.fillRoundedRectangle(axis, 4.f);
        g.setColour(colour.withAlpha(primary ? 1.f : 0.32f));
        g.drawRoundedRectangle(axis, 4.f, primary ? 1.8f : 1.f);

        if (primary) {
            g.setColour(colour.withAlpha(0.95f));
            g.fillRoundedRectangle(axis.reduced(6.f, 6.f), 2.f);
        }

        g.setColour(colour.withAlpha(linked ? 0.38f : 0.055f));
        g.fillRoundedRectangle(link, 4.f);
        g.setColour(colour.withAlpha(linked ? 0.96f : 0.32f));
        g.drawRoundedRectangle(link, 4.f, linked ? 1.8f : 1.f);
    }
}

}

Effect2DExpandedEditorComponent::Effect2DExpandedEditorComponent(Effect2DWidget& targetWidget) :
        widget(targetWidget) {
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
    addAndMakeVisible(controls.enabled);
    addAndMakeVisible(controls.firstSlider);
    addAndMakeVisible(controls.secondSlider);
    addAndMakeVisible(controls.thirdSlider);
    addAndMakeVisible(controls.menu);
    addAndMakeVisible(controls.firstButton);
    addAndMakeVisible(controls.secondButton);
    addAndMakeVisible(controls.thirdButton);
    addAndMakeVisible(controls.firstLabel);
    addAndMakeVisible(controls.secondLabel);
    addAndMakeVisible(controls.thirdLabel);
    addAndMakeVisible(controls.menuLabel);
}

Effect2DExpandedEditorComponent::~Effect2DExpandedEditorComponent() = default;

void Effect2DExpandedEditorComponent::setCallbacks(Callbacks nextCallbacks) {
    callbacks = std::move(nextCallbacks);

    auto safeThis = Component::SafePointer<Effect2DExpandedEditorComponent>(this);
    widget.setExpandedPanelCallbacks(
            [safeThis] {
                if (safeThis != nullptr) {
                    safeThis->repaint();

                    if (safeThis->callbacks.repaintOpenGL != nullptr) {
                        safeThis->callbacks.repaintOpenGL();
                    }
                }
            },
            [safeThis](const MouseCursor& cursor) {
                if (safeThis != nullptr) {
                    safeThis->setMouseCursor(cursor);
                }
            });
}

void Effect2DExpandedEditorComponent::setNode(const Node& nextNode) {
    node = nextNode;
    if (configuredKind != node.kind) {
        configureControls();
    }
    updatePanelHost();
    updateControlLayout();
    repaint();
}

void Effect2DExpandedEditorComponent::renderOpenGL(float scaleFactor) {
    if (node.kind != NodeKind::Envelope
            && node.kind != NodeKind::GuideCurve
            && node.kind != NodeKind::ImpulseResponse
            && node.kind != NodeKind::Waveshaper) {
        return;
    }

    widget.renderExpandedPanelOpenGL(
            node,
            panelBounds().translated((float) getX(), (float) getY()),
            getLocalBounds().toFloat().translated((float) getX(), (float) getY()),
            scaleFactor);
}

void Effect2DExpandedEditorComponent::paint(Graphics& g) {
    Rectangle<float> panel = getLocalBounds().toFloat();
    const Rectangle<int> panelHole = panelBounds().toNearestInt();

    g.saveState();
    g.excludeClipRegion(panelHole);
    g.setColour(Colours::black.withAlpha(0.38f));
    g.fillRoundedRectangle(panel.translated(0.f, 10.f), 8.f);
    g.setColour(Colour(0xff141a21));
    g.fillRoundedRectangle(panel, 8.f);
    g.restoreState();

    g.setColour(Colour(0xff17d0c5).withAlpha(0.72f));
    g.drawRoundedRectangle(panel, 8.f, 1.5f);

    auto header = panel.removeFromTop(kHeaderHeight);
    g.setColour(Colour(0xff202833));
    g.fillRoundedRectangle(header, 8.f);
    g.fillRect(header.withTrimmedTop(header.getHeight() - 8.f));

    g.setColour(kText);
    g.setFont(FontOptions(14.f, Font::bold));
    g.drawText(node.title, header.reduced(13.f, 4.f), Justification::centredLeft);

    if (node.kind == NodeKind::Envelope) {
        const auto controlsArea = controlBounds().toFloat();
        drawEnvelopeMorphSquare(
                g,
                envelopeMorphSquareBounds(controlsArea),
                (float) controls.firstSlider.getValue(),
                (float) controls.secondSlider.getValue());
        drawEnvelopeMorphButtonRow(g, controlsArea);

        std::array<String, 6> guideLabels {};
        TrimeshSidePanelRenderer::drawVertexParameters(
                g,
                envelopeVertexParameterBounds(controlsArea),
                widget.selectedVertexParameters(),
                guideLabels);
    }

    Rectangle<float> closeButton = closeButtonBounds();
    g.setColour(Colour(0xff0e1318));
    g.fillEllipse(closeButton);
    g.setColour(Colour(0xff354050));
    g.drawEllipse(closeButton, 1.f);
    g.setColour(kText);
    g.drawLine(closeButton.getX() + 7.f, closeButton.getY() + 7.f,
               closeButton.getRight() - 7.f, closeButton.getBottom() - 7.f, 1.4f);
    g.drawLine(closeButton.getRight() - 7.f, closeButton.getY() + 7.f,
               closeButton.getX() + 7.f, closeButton.getBottom() - 7.f, 1.4f);
}

void Effect2DExpandedEditorComponent::resized() {
    updatePanelHost();
    updateControlLayout();
}

void Effect2DExpandedEditorComponent::mouseMove(const MouseEvent& event) {
    setMouseCursor((node.kind == NodeKind::Envelope
                    && envelopeMorphPlaneBounds(controlBounds().toFloat()).contains(event.position))
            ? MouseCursor::CrosshairCursor
            : closeButtonBounds().contains(event.position)
            ? MouseCursor::PointingHandCursor
            : MouseCursor::NormalCursor);
}

void Effect2DExpandedEditorComponent::mouseDown(const MouseEvent& event) {
    draggingEnvelopeMorphPlane = false;

    if (closeButtonBounds().contains(event.position) && callbacks.close != nullptr) {
        callbacks.close();
        return;
    }

    if (node.kind == NodeKind::Envelope && updateEnvelopeMorphFromPoint(event.position)) {
        draggingEnvelopeMorphPlane = true;
    }
}

void Effect2DExpandedEditorComponent::mouseUp(const MouseEvent& event) {
    ignoreUnused(event);
    draggingEnvelopeMorphPlane = false;
}

void Effect2DExpandedEditorComponent::mouseDrag(const MouseEvent& event) {
    if (draggingEnvelopeMorphPlane && node.kind == NodeKind::Envelope) {
        updateEnvelopeMorphFromPoint(event.position);
    }
}

Rectangle<float> Effect2DExpandedEditorComponent::closeButtonBounds() const {
    const Rectangle<float> bounds = getLocalBounds().toFloat();
    return Rectangle<float>(22.f, 22.f).withCentre({ bounds.getRight() - 22.f, kHeaderHeight * 0.5f });
}

Rectangle<float> Effect2DExpandedEditorComponent::contentBounds() const {
    Rectangle<float> bounds = getLocalBounds().toFloat();
    bounds.removeFromTop(kHeaderHeight);
    return bounds.reduced(12.f, 10.f);
}

Rectangle<float> Effect2DExpandedEditorComponent::panelBounds() const {
    Rectangle<float> bounds = contentBounds();

    if (node.kind == NodeKind::Envelope) {
        bounds.removeFromTop(kEnvelopeTopControlsHeight);
        return bounds;
    }

    if (usesRightControlRail(node.kind)) {
        bounds.removeFromRight((float) rightControlRailWidth(node.kind));

        if (node.kind == NodeKind::Waveshaper) {
            const float size = jmin(bounds.getWidth(), bounds.getHeight());
            return Rectangle<float>(size, size).withCentre(bounds.getCentre());
        }

        return bounds;
    }

    return bounds.withTrimmedBottom(104.f);
}

Rectangle<float> Effect2DExpandedEditorComponent::panelBoundsForAutomation() const {
    return panelBounds();
}

var Effect2DExpandedEditorComponent::automationState() const {
    auto* root = new DynamicObject();
    root->setProperty("panelBounds", rectangleToVar(panelBounds()));
    root->setProperty("controlBounds", rectangleToVar(controlBounds().toFloat()));
    root->setProperty("vertexCount", widget.vertexCountForAutomation());
    root->setProperty("enabled", controls.enabled.getToggleState());
    root->setProperty("firstSlider", controls.firstSlider.getValue());
    root->setProperty("secondSlider", controls.secondSlider.getValue());
    root->setProperty("thirdSlider", controls.thirdSlider.getValue());
    root->setProperty("menuText", controls.menu.getText());
    root->setProperty("menuSelectedId", controls.menu.getSelectedId());
    root->setProperty("panelState", widget.automationState());
    return root;
}

Rectangle<int> Effect2DExpandedEditorComponent::controlBounds() const {
    Rectangle<float> bounds = contentBounds();

    if (node.kind == NodeKind::Envelope) {
        return bounds.removeFromTop(kEnvelopeTopControlsHeight).toNearestInt();
    }

    if (usesRightControlRail(node.kind)) {
        return bounds.removeFromRight((float) rightControlRailWidth(node.kind)).toNearestInt();
    }

    bounds.removeFromTop(panelBounds().getBottom() - bounds.getY() + 12.f);
    return bounds.toNearestInt();
}

void Effect2DExpandedEditorComponent::configureControls() {
    configuredKind = node.kind;
    controls.enabled.setButtonText("Enable");
    controls.enabled.setToggleState(true, dontSendNotification);
    controls.enabled.setVisible(true);
    controls.enabled.onClick = [this] {
        pushControlValues();
        repaint();
    };
    controls.firstSlider.onValueChange = [this] { pushControlValues(); };
    controls.secondSlider.onValueChange = [this] { pushControlValues(); };
    controls.thirdSlider.onValueChange = [this] { pushControlValues(); };
    controls.menu.onChange = [this] { pushControlValues(); };

    controls.firstSlider.setVisible(false);
    controls.secondSlider.setVisible(false);
    controls.thirdSlider.setVisible(false);
    controls.firstLabel.setVisible(false);
    controls.secondLabel.setVisible(false);
    controls.thirdLabel.setVisible(false);
    controls.menu.setVisible(false);
    controls.menuLabel.setVisible(false);
    controls.firstButton.setVisible(false);
    controls.secondButton.setVisible(false);
    controls.thirdButton.setVisible(false);

    if (node.kind == NodeKind::Envelope) {
        controls.enabled.setVisible(false);
        styleSlider(controls.firstSlider, controls.firstLabel, "Red");
        styleSlider(controls.secondSlider, controls.secondLabel, "Blue");
        controls.thirdSlider.setVisible(false);
        controls.thirdLabel.setVisible(false);
        styleMenu({}, {}, {});
        controls.menuLabel.setText("morph Position", dontSendNotification);
        controls.menuLabel.setColour(Label::textColourId, kMutedText);
        controls.menuLabel.setJustificationType(Justification::centred);
        controls.menuLabel.setVisible(true);
    } else if (node.kind == NodeKind::Waveshaper) {
        styleSlider(controls.firstSlider, controls.firstLabel, "Pre");
        styleSlider(controls.secondSlider, controls.secondLabel, "Post");
        controls.thirdSlider.setVisible(false);
        controls.thirdLabel.setVisible(false);
        styleMenu({ "1", "2", "4", "8" }, "1", "AA factor");
        controls.firstButton.setVisible(false);
        controls.secondButton.setVisible(false);
        controls.thirdButton.setVisible(false);
    } else if (node.kind == NodeKind::ImpulseResponse) {
        styleSlider(controls.firstSlider, controls.firstLabel, "Size");
        styleSlider(controls.secondSlider, controls.secondLabel, "Post");
        styleSlider(controls.thirdSlider, controls.thirdLabel, "HighPass");
        styleMenu({}, {}, {});
        styleButton(controls.firstButton, "Load");
        styleButton(controls.secondButton, "Unload");
        styleButton(controls.thirdButton, "Model");
    } else if (node.kind == NodeKind::GuideCurve) {
        styleSlider(controls.firstSlider, controls.firstLabel, "Noise");
        styleSlider(controls.secondSlider, controls.secondLabel, "DC Offset");
        styleSlider(controls.thirdSlider, controls.thirdLabel, "Phase");
        styleMenu({}, {}, {});
        styleButton(controls.firstButton, "+");
        styleButton(controls.secondButton, "-");
        controls.thirdButton.setVisible(false);
    }

    pushControlValues();
}

void Effect2DExpandedEditorComponent::styleSlider(Slider& slider, Label& label, const String& text) {
    label.setText(text, dontSendNotification);
    label.setColour(Label::textColourId, kMutedText);
    label.setJustificationType(Justification::centred);
    label.setVisible(true);

    if (node.kind == NodeKind::Envelope) {
        slider.setSliderStyle(Slider::LinearHorizontal);
        slider.setLookAndFeel(&morphRailLookAndFeel());
        slider.setColour(Slider::thumbColourId, envelopeAxisColour(text));
    } else {
        slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        slider.setLookAndFeel(&legacyKnobLookAndFeel());
        slider.setColour(Slider::thumbColourId, Colour(0xfff2f4f7));
    }

    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    slider.setRange(0.0, 1.0, 0.001);
    if (slider.getValue() <= slider.getMinimum() || slider.getValue() >= slider.getMaximum()) {
        slider.setValue(0.5, dontSendNotification);
    }
    slider.setColour(Slider::rotarySliderOutlineColourId, Colour(0xff4a5361));
    slider.setColour(Slider::rotarySliderFillColourId, Colour(0xffd9dde5));
    slider.setVisible(true);
}

void Effect2DExpandedEditorComponent::styleButton(Button& button, const String& text) {
    button.setButtonText(text);
    button.setColour(TextButton::buttonColourId, Colour(0xff161d25));
    button.setColour(TextButton::buttonOnColourId, Colour(0xff252f3b));
    button.setColour(TextButton::textColourOffId, kText);
    button.setVisible(true);
}

void Effect2DExpandedEditorComponent::styleMenu(
        const StringArray& items,
        const String& selected,
        const String& labelText) {
    controls.menu.clear(dontSendNotification);
    controls.menuLabel.setText(labelText, dontSendNotification);
    controls.menuLabel.setColour(Label::textColourId, kMutedText);
    controls.menuLabel.setJustificationType(Justification::centred);

    for (int i = 0; i < items.size(); ++i) {
        controls.menu.addItem(items[i], i + 1);
        if (items[i] == selected) {
            controls.menu.setSelectedId(i + 1, dontSendNotification);
        }
    }

    controls.menu.setVisible(!items.isEmpty());
    controls.menuLabel.setVisible(!items.isEmpty());
}

void Effect2DExpandedEditorComponent::updatePanelHost() {
    if (getWidth() <= 0 || getHeight() <= 0) {
        return;
    }

    Component* panel = widget.getExpandedPanelComponentIfCreated();

    if (panel == nullptr) {
        panel = widget.prepareExpandedPanelComponent(node, contentBounds());
    }

    if (panel == nullptr) {
        return;
    }

    const Rectangle<int> bounds = panelBounds().toNearestInt();

    if (panel->getParentComponent() != this) {
        addAndMakeVisible(panel);
    }

    if (panel->getBounds() != bounds) {
        panel->setBounds(bounds);
    }

    panel->setVisible(true);
    panel->toFront(false);
    panel->repaint();
}

void Effect2DExpandedEditorComponent::updateControlLayout() {
    Rectangle<int> bounds = controlBounds();
    bounds.reduce(4, 0);

    if (bounds.isEmpty()) {
        controls.enabled.setBounds({});
        controls.firstSlider.setBounds({});
        controls.secondSlider.setBounds({});
        controls.thirdSlider.setBounds({});
        controls.menu.setBounds({});
        controls.firstButton.setBounds({});
        controls.secondButton.setBounds({});
        controls.thirdButton.setBounds({});
        controls.firstLabel.setBounds({});
        controls.secondLabel.setBounds({});
        controls.thirdLabel.setBounds({});
        controls.menuLabel.setBounds({});
        return;
    }

    if (usesRightControlRail(node.kind)) {
        bounds.reduce(8, 8);

        if (node.kind == NodeKind::Envelope) {
            controls.enabled.setBounds({});

            Label* labels[3] {
                    &controls.firstLabel,
                    &controls.secondLabel,
                    &controls.thirdLabel
            };
            Slider* sliders[3] {
                    &controls.firstSlider,
                    &controls.secondSlider,
                    &controls.thirdSlider
            };

            bounds.removeFromTop(22);
            controls.menuLabel.setBounds(bounds.removeFromTop(22));
            bounds.removeFromTop(10);

            for (int i = 0; i < 3; ++i) {
                Rectangle<int> row = bounds.removeFromTop(35);
                labels[i]->setBounds(row.removeFromLeft(46));
                sliders[i]->setBounds(row);
                bounds.removeFromTop(9);
            }

            controls.menu.setBounds({});
            controls.firstButton.setBounds({});
            controls.secondButton.setBounds({});
            controls.thirdButton.setBounds({});
            return;
        }

        controls.enabled.setBounds(bounds.removeFromTop(32));
        bounds.removeFromTop(10);

        auto placeGuideSlider = [&](Slider& slider, Label& label) {
            Rectangle<int> item = bounds.removeFromTop(62);
            slider.setBounds(item.removeFromTop(42));
            label.setBounds(item);
            bounds.removeFromTop(7);
        };

        placeGuideSlider(controls.firstSlider, controls.firstLabel);
        placeGuideSlider(controls.secondSlider, controls.secondLabel);

        if (controls.thirdSlider.isVisible()) {
            placeGuideSlider(controls.thirdSlider, controls.thirdLabel);
        } else {
            controls.thirdSlider.setBounds({});
            controls.thirdLabel.setBounds({});
        }

        if (node.kind == NodeKind::ImpulseResponse) {
            auto placeRailButton = [&](Button& button) {
                button.setBounds(bounds.removeFromTop(34).reduced(3, 3));
                bounds.removeFromTop(4);
            };
            placeRailButton(controls.firstButton);
            placeRailButton(controls.secondButton);
            placeRailButton(controls.thirdButton);
        } else if (node.kind == NodeKind::Waveshaper) {
            if (controls.menu.isVisible()) {
                Rectangle<int> menuArea = bounds.removeFromTop(66);
                controls.menu.setBounds(menuArea.removeFromTop(42).reduced(3, 3));
                controls.menuLabel.setBounds(menuArea);
            } else {
                controls.menu.setBounds({});
                controls.menuLabel.setBounds({});
            }

            controls.firstButton.setBounds({});
            controls.secondButton.setBounds({});
            controls.thirdButton.setBounds({});
        } else {
            Rectangle<int> buttons = bounds.removeFromTop(36);
            controls.firstButton.setBounds(buttons.removeFromLeft(44).reduced(4, 3));
            controls.secondButton.setBounds(buttons.removeFromLeft(44).reduced(4, 3));
            controls.thirdButton.setBounds({});
        }

        if (node.kind != NodeKind::Waveshaper) {
            controls.menu.setBounds({});
            controls.menuLabel.setBounds({});
        }
        return;
    }

    if (node.kind == NodeKind::Envelope) {
        controls.enabled.setBounds({});
        controls.thirdSlider.setBounds({});
        controls.thirdLabel.setBounds({});
        controls.menu.setBounds({});
        controls.firstButton.setBounds({});
        controls.secondButton.setBounds({});
        controls.thirdButton.setBounds({});

        Rectangle<int> column = envelopeMorphRailColumnBounds(bounds.toFloat()).toNearestInt();
        column.removeFromTop((int) kEnvelopeSectionHeaderTopInset);
        controls.menuLabel.setBounds(column.removeFromTop((int) kEnvelopeSectionHeaderHeight));
        column.removeFromTop(18);

        auto placeMorphSlider = [&](Slider& slider, Label& label) {
            Rectangle<int> row = column.removeFromTop(35);
            label.setBounds(row.removeFromLeft(42));
            row.removeFromRight((int) (kEnvelopeAxisButtonSize * 2.f + kEnvelopeRowButtonGap * 3.f));
            slider.setBounds(row);
            column.removeFromTop(8);
        };

        placeMorphSlider(controls.firstSlider, controls.firstLabel);
        placeMorphSlider(controls.secondSlider, controls.secondLabel);
        return;
    }

    auto reserveRow = [&](int requestedWidth) {
        const int width = jmin(requestedWidth, bounds.getWidth());
        Rectangle<int> row(width, bounds.getHeight());
        row.setCentre(bounds.getCentre());
        return row;
    };

    const int visibleSliderCount = (controls.firstSlider.isVisible() ? 1 : 0)
            + (controls.secondSlider.isVisible() ? 1 : 0)
            + (controls.thirdSlider.isVisible() ? 1 : 0);
    const int visibleButtonCount = (controls.firstButton.isVisible() ? 1 : 0)
            + (controls.secondButton.isVisible() ? 1 : 0)
            + (controls.thirdButton.isVisible() ? 1 : 0);
    const int sliderWidth = node.kind == NodeKind::GuideCurve ? 72 : 90;
    const int buttonWidth = node.kind == NodeKind::GuideCurve ? 38 : 46;
    const int requestedWidth = 74
            + visibleSliderCount * sliderWidth
            + (controls.menu.isVisible() ? 98 : 0)
            + visibleButtonCount * buttonWidth
            + 10 * (2 + visibleSliderCount + visibleButtonCount);
    bounds = reserveRow(requestedWidth);

    const int verticalTrim = node.kind == NodeKind::GuideCurve ? 14 : 24;
    controls.enabled.setBounds(bounds.removeFromLeft(74).withTrimmedTop(verticalTrim).withTrimmedBottom(verticalTrim));
    bounds.removeFromLeft(10);

    auto placeSlider = [&](Slider& slider, Label& label) {
        Rectangle<int> item = bounds.removeFromLeft(sliderWidth);
        slider.setBounds(item.removeFromTop(node.kind == NodeKind::GuideCurve ? 46 : 68));
        label.setBounds(item);
        bounds.removeFromLeft(10);
    };

    if (controls.firstSlider.isVisible()) {
        placeSlider(controls.firstSlider, controls.firstLabel);
    }

    if (controls.secondSlider.isVisible()) {
        placeSlider(controls.secondSlider, controls.secondLabel);
    }

    if (controls.thirdSlider.isVisible()) {
        placeSlider(controls.thirdSlider, controls.thirdLabel);
    }

    if (controls.menu.isVisible()) {
        Rectangle<int> menuArea = bounds.removeFromLeft(98);
        controls.menu.setBounds(menuArea.removeFromTop(42).reduced(4, 3));
        controls.menuLabel.setBounds(menuArea);
        bounds.removeFromLeft(10);
    }

    auto placeButton = [&](Button& button) {
        if (!button.isVisible()) {
            return;
        }

        button.setBounds(bounds.removeFromLeft(buttonWidth).withTrimmedTop(verticalTrim).withTrimmedBottom(verticalTrim));
        bounds.removeFromLeft(6);
    };

    placeButton(controls.firstButton);
    placeButton(controls.secondButton);
    placeButton(controls.thirdButton);
}

bool Effect2DExpandedEditorComponent::updateEnvelopeMorphFromPoint(Point<float> position) {
    const Rectangle<float> square = envelopeMorphPlaneBounds(controlBounds().toFloat());

    if (!square.contains(position) && !draggingEnvelopeMorphPlane) {
        return false;
    }

    const float red = jlimit(0.f, 1.f, (position.x - square.getX()) / square.getWidth());
    const float blue = jlimit(0.f, 1.f, (square.getBottom() - position.y) / square.getHeight());

    controls.firstSlider.setValue(red, sendNotificationSync);
    controls.secondSlider.setValue(blue, sendNotificationSync);
    repaint();
    return true;
}

void Effect2DExpandedEditorComponent::pushControlValues() {
    widget.setControlValues(
            controls.enabled.getToggleState(),
            (float) controls.firstSlider.getValue(),
            (float) controls.secondSlider.getValue(),
            (float) controls.thirdSlider.getValue(),
            controls.menu.getSelectedId());

    if (callbacks.repaintOpenGL != nullptr) {
        callbacks.repaintOpenGL();
    }
}

}

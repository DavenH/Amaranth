#include "Effect2DExpandedEditorComponent.h"

#include "../Trimesh/TrimeshSidePanelRenderer.h"

#include <utility>

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kHeaderHeight = 34.f;
constexpr int kGuideControlWidth = 196;
constexpr int kIrControlWidth = 212;
constexpr int kWaveshaperControlWidth = 224;
constexpr float kEnvelopeTopControlsHeight = 246.f;
constexpr float kEnvelopeRowButtonGap = 6.f;
constexpr float kEnvelopeMorphSquareWidth = 178.f;
constexpr float kEnvelopeMorphGap = 22.f;
constexpr float kEnvelopeMorphRailWidth = 328.f;
constexpr float kEnvelopeVertexGap = 18.f;
constexpr float kEnvelopeSectionHeaderHeight = 18.f;
constexpr float kEnvelopeSectionHeaderTopInset = 5.f;
constexpr float kEnvelopeAxisButtonSize = 20.f;
constexpr float kEnvelopeMorphRowHeight = 32.f;
constexpr float kEnvelopeMorphRowStep = 35.f;

struct RightRailLayoutProfile {
    int width {};
    int controlGroupHeight {};
    int enabledHeight {};
    int enabledGap {};
    int sliderItemHeight {};
    int sliderHeight {};
    int sliderGap {};
    int menuTopGap {};
    int menuAreaHeight {};
    int menuLabelHeight {};
    int menuHeight {};
    int menuHorizontalInset {};
    int menuVerticalInset {};
    float panelInsetX {};
    float panelInsetY {};
    float maxSquarePanelSize {};
    bool squarePanel {};
};

RightRailLayoutProfile rightRailProfile(NodeKind kind) {
    if (kind == NodeKind::Waveshaper) {
        return {
                kWaveshaperControlWidth,
                164,
                26,
                14,
                30,
                24,
                10,
                10,
                30,
                0,
                30,
                4,
                2,
                18.f,
                14.f,
                300.f,
                true
        };
    }

    if (kind == NodeKind::ImpulseResponse) {
        return { kIrControlWidth, 0, 30, 10, 62, 42, 7 };
    }

    return { kGuideControlWidth, 0, 30, 10, 62, 42, 7 };
}

bool usesRightControlRail(NodeKind kind) {
    return kind == NodeKind::GuideCurve
            || kind == NodeKind::ImpulseResponse
            || kind == NodeKind::Waveshaper;
}

int rightControlRailWidth(NodeKind kind) {
    return rightRailProfile(kind).width;
}

Colour envelopeAxisColour(const String& label) {
    if (label == "Time") {
        return Colour(0xffd7bf5f);
    }

    if (label == "Red") {
        return Colour(0xffd65a5a);
    }

    if (label == "Blue") {
        return Colour(0xff5f91e8);
    }

    return Colour(0xffd8d5ca);
}

String envelopeViewAxisLabel(int axisIndex) {
    switch (axisIndex) {
        case 0:     return "Time";
        case 1:     return "Red";
        case 2:     return "Blue";
        default:    return {};
    }
}

bool boolParameterValue(const Node& node, const String& parameterId, bool fallback) {
    const String value = parameterValueForNode(node, parameterId, fallback ? "1" : "0").toLowerCase();
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

Colour envelopeViewAxisColour(int axisIndex) {
    return envelopeAxisColour(envelopeViewAxisLabel(axisIndex));
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

class EffectRailSliderLookAndFeel :
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
        ignoreUnused(minSliderPos, maxSliderPos, style, slider);

        auto row = Rectangle<float>((float) x, (float) y, (float) width, (float) height);
        auto rail = row.withSizeKeepingCentre(row.getWidth(), 7.f);
        const float knobX = jlimit(rail.getX(), rail.getRight(), sliderPos);

        g.setColour(Colour(0xff384351));
        g.fillRoundedRectangle(rail, 3.5f);
        g.setColour(Colour(0xffdce3ec).withAlpha(0.78f));
        g.fillRoundedRectangle(rail.withRight(knobX), 3.5f);
        g.setColour(Colour(0xff0d1116));
        g.fillEllipse(Rectangle<float>(16.f, 16.f).withCentre({ knobX, rail.getCentreY() }));
        g.setColour(Colour(0xffdce3ec));
        g.drawEllipse(Rectangle<float>(16.f, 16.f).withCentre({ knobX, rail.getCentreY() }), 1.5f);
    }
};

EffectRailSliderLookAndFeel& effectRailSliderLookAndFeel() {
    static EffectRailSliderLookAndFeel lookAndFeel;
    return lookAndFeel;
}

class PowerToggleLookAndFeel :
        public LookAndFeel_V4 {
public:
    void drawToggleButton(
            Graphics& g,
            ToggleButton& button,
            bool shouldDrawButtonAsHighlighted,
            bool shouldDrawButtonAsDown) override {
        ignoreUnused(shouldDrawButtonAsDown);

        const auto bounds = button.getLocalBounds().toFloat().reduced(2.f);
        const Point<float> centre = bounds.getCentre();
        const float radius = jmin(bounds.getWidth(), bounds.getHeight()) * 0.34f;
        const Colour colour = button.getToggleState() ? Colour(0xffdce3ec) : Colour(0xff697584);
        const float alpha = shouldDrawButtonAsHighlighted ? 1.f : 0.82f;

        g.setColour(Colour(0xff0d1116));
        g.fillEllipse(bounds);
        g.setColour(Colour(0xff435061));
        g.drawEllipse(bounds, 1.f);

        Path arc;
        arc.addCentredArc(
                centre.x,
                centre.y,
                radius,
                radius,
                0.f,
                MathConstants<float>::pi * 0.28f,
                MathConstants<float>::pi * 1.72f,
                true);
        g.setColour(colour.withAlpha(alpha));
        g.strokePath(arc, PathStrokeType(2.1f, PathStrokeType::curved, PathStrokeType::rounded));
        g.drawLine(centre.x, centre.y - radius * 1.15f, centre.x, centre.y - radius * 0.2f, 2.1f);
    }
};

PowerToggleLookAndFeel& powerToggleLookAndFeel() {
    static PowerToggleLookAndFeel lookAndFeel;
    return lookAndFeel;
}

Image legacyEnvelopeIconSheet() {
    static Image sheet = [] {
      #if defined(CYCLE_V2_SOURCE_DIR)
        const File iconFile = File(String(CYCLE_V2_SOURCE_DIR))
                .getChildFile("resources")
                .getChildFile("icons.png");

        if (iconFile.existsAsFile()) {
            return ImageFileFormat::loadFrom(iconFile);
        }
      #endif

        return Image {};
    }();

    return sheet;
}

bool legacyEnvelopeMarkerCell(const String& label, int& column, int& row) {
    if (label == "Loop") {
        column = 4;
        row    = 3;
        return true;
    }

    if (label == "Sustain") {
        column = 5;
        row    = 3;
        return true;
    }

    return false;
}

class LegacyEnvelopeMarkerButtonLookAndFeel :
        public LookAndFeel_V4 {
public:
    void drawButtonBackground(
            Graphics& g,
            Button& button,
            const Colour& backgroundColour,
            bool shouldDrawButtonAsHighlighted,
            bool shouldDrawButtonAsDown) override {
        ignoreUnused(backgroundColour);

        auto bounds = button.getLocalBounds().toFloat().reduced(1.5f);
        const bool active = button.getToggleState();
        const Colour outline = active ? Colour(0xffdce3ec) : Colour(0xff7d8998);
        const Colour fill = active ? Colour(0xff26313d) : Colour(0xff111821);

        g.setColour(fill.withAlpha(shouldDrawButtonAsDown ? 0.98f : 0.82f));
        g.fillRoundedRectangle(bounds, 6.f);
        g.setColour(outline.withAlpha(shouldDrawButtonAsHighlighted ? 0.96f : 0.72f));
        g.drawRoundedRectangle(bounds, 6.f, active ? 1.7f : 1.25f);
    }

    void drawButtonText(
            Graphics& g,
            TextButton& button,
            bool shouldDrawButtonAsHighlighted,
            bool shouldDrawButtonAsDown) override {
        int column {};
        int row {};
        if (!legacyEnvelopeMarkerCell(button.getButtonText(), column, row)) {
            LookAndFeel_V4::drawButtonText(g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
            return;
        }

        const Image sheet = legacyEnvelopeIconSheet();
        if (sheet.isNull()) {
            LookAndFeel_V4::drawButtonText(g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
            return;
        }

        constexpr int cellSize = 24;
        const Rectangle<int> source(column * cellSize, row * cellSize, cellSize, cellSize);
        const float iconSize = jmin(21.f, (float) jmin(button.getWidth(), button.getHeight()) - 7.f);
        Rectangle<float> target(iconSize, iconSize);
        target.setCentre(button.getLocalBounds().toFloat().getCentre());

        const float alpha = button.getToggleState()
                ? 1.f
                : (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown ? 0.9f : 0.68f);
        g.setOpacity(alpha);
        g.drawImage(
                sheet,
                target.getX(),
                target.getY(),
                target.getWidth(),
                target.getHeight(),
                source.getX(),
                source.getY(),
                source.getWidth(),
                source.getHeight());
        g.setOpacity(1.f);
    }
};

LegacyEnvelopeMarkerButtonLookAndFeel& legacyEnvelopeMarkerButtonLookAndFeel() {
    static LegacyEnvelopeMarkerButtonLookAndFeel lookAndFeel;
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
    column.translate(0.f, (float) axisIndex * kEnvelopeMorphRowStep);
    return column.removeFromTop(kEnvelopeMorphRowHeight);
}

Rectangle<float> envelopeMorphRailBounds(Rectangle<float> controls, int axisIndex) {
    Rectangle<float> row = envelopeMorphRowBounds(controls, axisIndex);
    row.removeFromLeft(42.f);
    row.removeFromRight(kEnvelopeAxisButtonSize * 2.f + kEnvelopeRowButtonGap * 3.f);
    return row;
}

Rectangle<float> envelopeMarkerGroupBounds(Rectangle<float> controls) {
    Rectangle<float> column = envelopeMorphRailColumnBounds(controls);
    column.removeFromTop(
            kEnvelopeSectionHeaderTopInset
                    + kEnvelopeSectionHeaderHeight
                    + 18.f
                    + 3.f * kEnvelopeMorphRowStep
                    + 14.f);
    return column.removeFromTop(64.f);
}

struct EnvelopeMarkerButtonLayout {
    Rectangle<int> loop;
    Rectangle<int> sustain;
    Rectangle<int> log;
};

EnvelopeMarkerButtonLayout envelopeMarkerButtonLayout(Rectangle<float> controls) {
    Rectangle<int> markerGroup = envelopeMarkerGroupBounds(controls).toNearestInt();
    markerGroup.removeFromTop(22);
    Rectangle<int> markerRow = markerGroup.removeFromTop(30);

    const int iconButtonWidth = jmin(32, markerRow.getHeight());
    const int logButtonWidth = 54;
    const int totalWidth = iconButtonWidth * 2 + logButtonWidth + 18;
    markerRow = markerRow.withSizeKeepingCentre(totalWidth, markerRow.getHeight());

    EnvelopeMarkerButtonLayout layout;
    layout.loop = markerRow.removeFromLeft(iconButtonWidth).reduced(2, 2);
    markerRow.removeFromLeft(9);
    layout.sustain = markerRow.removeFromLeft(iconButtonWidth).reduced(2, 2);
    markerRow.removeFromLeft(9);
    layout.log = markerRow.removeFromLeft(logButtonWidth).reduced(2, 2);
    return layout;
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

Rectangle<float> envelopeMorphHeaderBounds(Rectangle<float> controls) {
    Rectangle<float> column = envelopeMorphRailColumnBounds(controls);
    column.removeFromTop(kEnvelopeSectionHeaderTopInset);
    return column.removeFromTop(kEnvelopeSectionHeaderHeight);
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
            blueColour.withAlpha(0.40f),
            square.getX(),
            square.getCentreY(),
            blueColour.withAlpha(0.02f),
            square.getRight(),
            square.getCentreY(),
            false));
    g.fillRect(square);

    g.setGradientFill(ColourGradient(
            redColour.withAlpha(0.02f),
            square.getCentreX(),
            square.getY(),
            redColour.withAlpha(0.40f),
            square.getCentreX(),
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

void drawEnvelopeAxisAndLinkButtons(
        Graphics& g,
        Rectangle<float> controls,
        int selectedAxis,
        const bool* linkedAxes) {
    const Rectangle<float> axisHeader = envelopePrimaryAxisBounds(controls, 0);
    const Rectangle<float> linkHeader = envelopeLinkToggleBounds(controls, 0);
    const float headerY = envelopeMorphRowBounds(controls, 0).getY() - 13.f;

    g.setColour(kMutedText.withAlpha(0.78f));
    g.setFont(FontOptions(8.5f, Font::bold));
    g.drawText("axis", axisHeader.withY(headerY).withHeight(10.f), Justification::centred);
    g.drawText("link", linkHeader.withY(headerY).withHeight(10.f), Justification::centred);

    for (int i = 0; i < 3; ++i) {
        const bool selected = i == selectedAxis;
        const bool linkInteractive = i != 0;
        const bool linked = linkedAxes != nullptr && linkedAxes[i];
        const Colour colour = envelopeViewAxisColour(i);
        const Rectangle<float> axis = envelopePrimaryAxisBounds(controls, i);
        const Rectangle<float> link = envelopeLinkToggleBounds(controls, i);

        g.setColour(colour.withAlpha(selected ? 0.42f : 0.055f));
        g.fillRoundedRectangle(axis, 4.f);
        g.setColour(colour.withAlpha(selected ? 1.f : 0.32f));
        g.drawRoundedRectangle(axis, 4.f, selected ? 1.8f : 1.f);

        if (selected) {
            g.setColour(colour.withAlpha(0.95f));
            g.fillRoundedRectangle(axis.reduced(6.f, 6.f), 2.f);
        }

        g.setColour(colour.withAlpha(!linkInteractive ? 0.035f : linked ? 0.38f : 0.055f));
        g.fillRoundedRectangle(link, 4.f);
        g.setColour(colour.withAlpha(!linkInteractive ? 0.18f : linked ? 0.96f : 0.32f));
        g.drawRoundedRectangle(link, 4.f, linked ? 1.8f : 1.f);

        if (linked) {
            g.setColour(colour.withAlpha(linkInteractive ? 0.95f : 0.30f));
            Path chain;
            const float cy = link.getCentreY();
            chain.addRoundedRectangle(link.getX() + 4.f, cy - 3.f, 6.f, 6.f, 2.5f);
            chain.addRoundedRectangle(link.getRight() - 10.f, cy - 3.f, 6.f, 6.f, 2.5f);
            g.strokePath(chain, PathStrokeType(1.2f));
            g.drawLine(link.getX() + 9.f, cy, link.getRight() - 9.f, cy, 1.2f);
        }
    }
}

void drawEnvelopeMarkerHeader(Graphics& g, Rectangle<float> controls) {
    const auto layout = envelopeMarkerButtonLayout(controls);
    auto labelBounds = [](Rectangle<int> buttonBounds) {
        return buttonBounds.toFloat().withY((float) buttonBounds.getY() - 12.f).withHeight(10.f);
    };

    g.setColour(kMutedText);
    g.setFont(FontOptions(10.5f, Font::bold));
    g.drawText("loop", labelBounds(layout.loop), Justification::centred);
    g.drawText("sustain", labelBounds(layout.sustain).expanded(10.f, 0.f), Justification::centred);
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
    syncEnvelopeStateFromNode();
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
        drawEnvelopeAxisAndLinkButtons(g, controlsArea, envelopeViewAxis, envelopeAxisLinked);
        drawEnvelopeMarkerHeader(g, controlsArea);

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

    panel = getLocalBounds().toFloat();
    g.setColour(Colour(0xffa7b0bd).withAlpha(0.62f));
    g.drawRoundedRectangle(panel.reduced(0.75f), 8.f, 1.3f);
}

void Effect2DExpandedEditorComponent::resized() {
    updatePanelHost();
    updateControlLayout();
}

void Effect2DExpandedEditorComponent::mouseMove(const MouseEvent& event) {
    const Rectangle<float> controlsArea = controlBounds().toFloat();
    bool overEnvelopeViewAxis = false;
    for (int i = 0; i < 3; ++i) {
        overEnvelopeViewAxis = overEnvelopeViewAxis
                || envelopePrimaryAxisBounds(controlsArea, i).contains(event.position)
                || (i != 0 && envelopeLinkToggleBounds(controlsArea, i).contains(event.position));
    }

    String parameterId;
    float value {};
    Rectangle<int> guideBounds;
    const bool overVertexParameter = findVertexParameterAt(event.position, parameterId, value)
            || findVertexGuideAt(event.position, parameterId, guideBounds);

    setMouseCursor((node.kind == NodeKind::Envelope
                    && envelopeMorphPlaneBounds(controlsArea).contains(event.position))
            ? MouseCursor::CrosshairCursor
            : overVertexParameter
            ? MouseCursor::PointingHandCursor
            : (node.kind == NodeKind::Envelope && overEnvelopeViewAxis)
            ? MouseCursor::PointingHandCursor
            : closeButtonBounds().contains(event.position)
            ? MouseCursor::PointingHandCursor
            : MouseCursor::NormalCursor);
}

void Effect2DExpandedEditorComponent::mouseDown(const MouseEvent& event) {
    draggingEnvelopeMorphPlane = false;
    draggingVertexParameter = false;

    if (closeButtonBounds().contains(event.position) && callbacks.close != nullptr) {
        callbacks.close();
        return;
    }

    if (showVertexParameterGuideMenu(event.position)) {
        return;
    }

    if (beginVertexParameterEdit(event.position)) {
        return;
    }

    if (node.kind == NodeKind::Envelope && updateEnvelopeViewAxisFromPoint(event.position)) {
        return;
    }

    if (node.kind == NodeKind::Envelope && updateEnvelopeMorphFromPoint(event.position)) {
        draggingEnvelopeMorphPlane = true;
    }
}

void Effect2DExpandedEditorComponent::mouseUp(const MouseEvent& event) {
    ignoreUnused(event);
    draggingEnvelopeMorphPlane = false;
    draggingVertexParameter = false;
    activeVertexParameterId = {};
}

void Effect2DExpandedEditorComponent::mouseDrag(const MouseEvent& event) {
    if (draggingEnvelopeMorphPlane && node.kind == NodeKind::Envelope) {
        updateEnvelopeMorphFromPoint(event.position);
    }

    if (draggingVertexParameter) {
        updateVertexParameterEdit(event.position);
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
        const RightRailLayoutProfile profile = rightRailProfile(node.kind);
        bounds.removeFromRight((float) profile.width);

        if (profile.squarePanel) {
            bounds.reduce(profile.panelInsetX, profile.panelInsetY);
            const float size = jmin(bounds.getWidth(), bounds.getHeight());
            const float squareSize = jmin(size, profile.maxSquarePanelSize);
            return Rectangle<float>(squareSize, squareSize)
                    .withX(bounds.getX())
                    .withCentre({ bounds.getX() + squareSize * 0.5f, bounds.getCentreY() });
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
    root->setProperty("envelopeViewAxis", envelopeViewAxis);
    root->setProperty("envelopeLogarithmic", envelopeLogarithmic);
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
        const RightRailLayoutProfile profile = rightRailProfile(node.kind);

        if (profile.squarePanel) {
            Rectangle<float> rail = bounds.removeFromRight((float) profile.width);
            const Rectangle<float> panel = panelBounds();
            return rail.withY(panel.getY()).withHeight(panel.getHeight()).toNearestInt();
        }

        return bounds.removeFromRight((float) profile.width).toNearestInt();
    }

    bounds.removeFromTop(panelBounds().getBottom() - bounds.getY() + 12.f);
    return bounds.toNearestInt();
}

void Effect2DExpandedEditorComponent::configureControls() {
    configuredKind = node.kind;
    controls.enabled.setButtonText("Enable");
    controls.enabled.setLookAndFeel(nullptr);
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
    controls.firstButton.onClick = nullptr;
    controls.secondButton.onClick = nullptr;
    controls.thirdButton.onClick = nullptr;
    controls.firstButton.setLookAndFeel(nullptr);
    controls.secondButton.setLookAndFeel(nullptr);
    controls.thirdButton.setLookAndFeel(nullptr);
    controls.firstButton.setClickingTogglesState(false);
    controls.secondButton.setClickingTogglesState(false);
    controls.thirdButton.setClickingTogglesState(false);
    controls.firstButton.setToggleState(false, dontSendNotification);
    controls.secondButton.setToggleState(false, dontSendNotification);
    controls.thirdButton.setToggleState(false, dontSendNotification);

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
        envelopeViewAxis = 0;
        envelopeAxisLinked[0] = true;
        envelopeAxisLinked[1] = true;
        envelopeAxisLinked[2] = true;
        envelopeLogarithmic = boolParameterValue(node, "logarithmic", false);
        controls.enabled.setVisible(false);
        styleSlider(controls.firstSlider, controls.firstLabel, "Red");
        styleSlider(controls.secondSlider, controls.secondLabel, "Blue");
        controls.thirdSlider.setVisible(false);
        controls.thirdLabel.setVisible(false);
        styleMenu({}, {}, {});
        controls.menuLabel.setText("morph position", dontSendNotification);
        controls.menuLabel.setColour(Label::textColourId, kMutedText);
        controls.menuLabel.setFont(FontOptions(10.5f, Font::bold));
        controls.menuLabel.setJustificationType(Justification::centred);
        controls.menuLabel.setVisible(true);
        styleButton(controls.firstButton, "Loop");
        styleButton(controls.secondButton, "Sustain");
        controls.firstButton.setTooltip("Toggle selected vertex as loop start");
        controls.secondButton.setTooltip("Toggle selected vertex as sustain");
        controls.firstButton.setLookAndFeel(&legacyEnvelopeMarkerButtonLookAndFeel());
        controls.secondButton.setLookAndFeel(&legacyEnvelopeMarkerButtonLookAndFeel());
        controls.firstButton.onClick = [this] {
            widget.toggleSelectedEnvelopeMarker(true);
            updateEnvelopeMarkerButtons();

            if (callbacks.repaintOpenGL != nullptr) {
                callbacks.repaintOpenGL();
            }
        };
        controls.secondButton.onClick = [this] {
            widget.toggleSelectedEnvelopeMarker(false);
            updateEnvelopeMarkerButtons();

            if (callbacks.repaintOpenGL != nullptr) {
                callbacks.repaintOpenGL();
            }
        };
        styleButton(controls.thirdButton, "Log");
        controls.thirdButton.setClickingTogglesState(true);
        controls.thirdButton.setToggleState(envelopeLogarithmic, dontSendNotification);
        controls.thirdButton.onClick = [this] {
            envelopeLogarithmic = controls.thirdButton.getToggleState();
            if (callbacks.setNodeParameter != nullptr) {
                callbacks.setNodeParameter("logarithmic", "Logarithmic", envelopeLogarithmic ? "1" : "0");
            }
            widget.setEnvelopeLogarithmic(envelopeLogarithmic);

            if (callbacks.repaintOpenGL != nullptr) {
                callbacks.repaintOpenGL();
            }
        };
        widget.setEnvelopeLogarithmic(envelopeLogarithmic);
        syncEnvelopeAxisLinksToWidget();
        updateEnvelopeMarkerButtons();
    } else if (node.kind == NodeKind::Waveshaper) {
        controls.enabled.setButtonText({});
        controls.enabled.setLookAndFeel(&powerToggleLookAndFeel());
        styleSlider(controls.firstSlider, controls.firstLabel, "Pre");
        styleSlider(controls.secondSlider, controls.secondLabel, "Post");
        controls.thirdSlider.setVisible(false);
        controls.thirdLabel.setText("Enable", dontSendNotification);
        controls.thirdLabel.setColour(Label::textColourId, kMutedText);
        controls.thirdLabel.setJustificationType(Justification::centredRight);
        controls.thirdLabel.setFont(FontOptions(12.f, Font::bold));
        controls.thirdLabel.setVisible(true);
        styleMenu({ "1", "2", "4", "8" }, "1", "AA factor");
        controls.menuLabel.setJustificationType(Justification::centredRight);
        controls.menuLabel.setFont(FontOptions(12.f, Font::bold));
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
        label.setFont(FontOptions(13.f));
        label.setJustificationType(Justification::centredRight);
    } else if (usesRightControlRail(node.kind)) {
        slider.setSliderStyle(Slider::LinearHorizontal);
        slider.setLookAndFeel(&effectRailSliderLookAndFeel());
        slider.setColour(Slider::thumbColourId, Colour(0xffdce3ec));
        label.setJustificationType(Justification::centredRight);
        label.setFont(FontOptions(12.f, Font::bold));
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
    button.setColour(TextButton::textColourOnId, kText);
    button.setVisible(true);
}

void Effect2DExpandedEditorComponent::styleMenu(
        const StringArray& items,
        const String& selected,
        const String& labelText) {
    controls.menu.clear(dontSendNotification);
    controls.menuLabel.setText(labelText, dontSendNotification);
    controls.menuLabel.setColour(Label::textColourId, kMutedText);
    controls.menuLabel.setFont(FontOptions(13.f));
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

void Effect2DExpandedEditorComponent::updateEnvelopeMarkerButtons() {
    if (node.kind != NodeKind::Envelope) {
        return;
    }

    controls.firstButton.setToggleState(widget.selectedEnvelopeMarkerState(true), dontSendNotification);
    controls.secondButton.setToggleState(widget.selectedEnvelopeMarkerState(false), dontSendNotification);
    controls.thirdButton.setToggleState(envelopeLogarithmic, dontSendNotification);
}

void Effect2DExpandedEditorComponent::syncEnvelopeStateFromNode() {
    if (node.kind != NodeKind::Envelope) {
        return;
    }

    const bool nextLogarithmic = boolParameterValue(node, "logarithmic", false);
    if (envelopeLogarithmic == nextLogarithmic) {
        return;
    }

    envelopeLogarithmic = nextLogarithmic;
    controls.thirdButton.setToggleState(envelopeLogarithmic, dontSendNotification);
    widget.setEnvelopeLogarithmic(envelopeLogarithmic);
}

void Effect2DExpandedEditorComponent::syncEnvelopeAxisLinksToWidget() {
    if (node.kind != NodeKind::Envelope) {
        return;
    }

    widget.setEnvelopeAxisLinks(envelopeAxisLinked[1], envelopeAxisLinked[2]);
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

        const RightRailLayoutProfile profile = rightRailProfile(node.kind);

        if (profile.controlGroupHeight > 0) {
            bounds = bounds.withSizeKeepingCentre(
                    bounds.getWidth(),
                    jmin(bounds.getHeight(), profile.controlGroupHeight));
        }

        if (node.kind == NodeKind::Waveshaper) {
            const int labelWidth = 78;
            const int widgetWidth = 116;
            const int columnGap = 12;
            const int rowHeight = 34;
            const int rowGap = 13;
            const int gridWidth = labelWidth + columnGap + widgetWidth;

            bounds = bounds.withSizeKeepingCentre(
                    jmin(bounds.getWidth(), gridWidth),
                    jmin(bounds.getHeight(), 4 * rowHeight + 3 * rowGap));

            auto placeControlRow = [&](Label& label, Component& component, int componentHeight) {
                Rectangle<int> row = bounds.removeFromTop(rowHeight);
                label.setBounds(row.removeFromLeft(labelWidth));
                row.removeFromLeft(columnGap);
                component.setBounds(row.removeFromLeft(widgetWidth).withSizeKeepingCentre(
                        widgetWidth,
                        componentHeight));
                bounds.removeFromTop(rowGap);
            };

            Rectangle<int> enableRow = bounds.removeFromTop(rowHeight);
            controls.thirdLabel.setBounds(enableRow.removeFromLeft(labelWidth));
            enableRow.removeFromLeft(columnGap);
            controls.enabled.setBounds(Rectangle<int>(34, 34).withCentre(
                    enableRow.removeFromLeft(widgetWidth).getCentre()));
            bounds.removeFromTop(rowGap);

            placeControlRow(controls.firstLabel, controls.firstSlider, 26);
            placeControlRow(controls.secondLabel, controls.secondSlider, 26);
            controls.thirdSlider.setBounds({});

            Rectangle<int> menuRow = bounds.removeFromTop(rowHeight);
            controls.menuLabel.setBounds(menuRow.removeFromLeft(labelWidth));
            menuRow.removeFromLeft(columnGap);
            controls.menu.setBounds(menuRow.removeFromLeft(76).withSizeKeepingCentre(76, 32));
            controls.firstButton.setBounds({});
            controls.secondButton.setBounds({});
            controls.thirdButton.setBounds({});
            return;
        }

        const int labelWidth = node.kind == NodeKind::ImpulseResponse ? 78 : 82;
        const int columnGap = 12;
        const int rowHeight = 32;
        const int rowGap = 12;

        auto placeRailSlider = [&](Slider& slider, Label& label) {
            Rectangle<int> row = bounds.removeFromTop(rowHeight);
            label.setBounds(row.removeFromLeft(labelWidth));
            row.removeFromLeft(columnGap);
            slider.setBounds(row.withTrimmedRight(4).withSizeKeepingCentre(row.getWidth() - 4, 26));
            bounds.removeFromTop(rowGap);
        };

        controls.enabled.setBounds(bounds.removeFromTop(30).withTrimmedLeft(labelWidth + columnGap));
        bounds.removeFromTop(rowGap);

        placeRailSlider(controls.firstSlider, controls.firstLabel);
        placeRailSlider(controls.secondSlider, controls.secondLabel);

        if (controls.thirdSlider.isVisible()) {
            placeRailSlider(controls.thirdSlider, controls.thirdLabel);
        } else {
            controls.thirdSlider.setBounds({});
            controls.thirdLabel.setBounds({});
        }

        if (node.kind == NodeKind::ImpulseResponse) {
            auto placeRailButton = [&](Button& button, int width) {
                Rectangle<int> row = bounds.removeFromTop(32);
                row.removeFromLeft(labelWidth + columnGap);
                button.setBounds(row.removeFromLeft(width).reduced(0, 2));
                bounds.removeFromTop(7);
            };
            placeRailButton(controls.firstButton, 76);
            placeRailButton(controls.secondButton, 76);
            placeRailButton(controls.thirdButton, 76);
        } else if (node.kind == NodeKind::Waveshaper) {
            if (controls.menu.isVisible()) {
                bounds.removeFromTop(profile.menuTopGap);
                Rectangle<int> menuArea = bounds.removeFromTop(profile.menuAreaHeight);
                controls.menuLabel.setBounds(menuArea.removeFromTop(profile.menuLabelHeight));
                controls.menu.setBounds(menuArea.removeFromTop(profile.menuHeight).reduced(
                        profile.menuHorizontalInset,
                        profile.menuVerticalInset));
            } else {
                controls.menu.setBounds({});
                controls.menuLabel.setBounds({});
            }

            controls.firstButton.setBounds({});
            controls.secondButton.setBounds({});
            controls.thirdButton.setBounds({});
        } else {
            Rectangle<int> buttons = bounds.removeFromTop(36);
            buttons.removeFromLeft(labelWidth + columnGap);
            controls.firstButton.setBounds(buttons.removeFromLeft(44).reduced(2, 3));
            buttons.removeFromLeft(8);
            controls.secondButton.setBounds(buttons.removeFromLeft(44).reduced(2, 3));
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
        controls.thirdButton.setBounds({});

        controls.menuLabel.setBounds(envelopeMorphHeaderBounds(bounds.toFloat()).toNearestInt());
        controls.thirdLabel.setText("Time", dontSendNotification);
        controls.thirdLabel.setColour(Label::textColourId, kMutedText);
        controls.thirdLabel.setJustificationType(Justification::centredRight);
        controls.thirdLabel.setFont(FontOptions(13.f));
        controls.thirdLabel.setVisible(true);
        controls.thirdLabel.setBounds(envelopeMorphRowBounds(bounds.toFloat(), 0)
                .withWidth(42.f)
                .toNearestInt());

        auto placeMorphSlider = [&](Slider& slider, Label& label, int axisIndex) {
            Rectangle<int> row = envelopeMorphRowBounds(bounds.toFloat(), axisIndex).toNearestInt();
            label.setBounds(row.removeFromLeft(42));
            row.removeFromRight((int) (kEnvelopeAxisButtonSize * 2.f + kEnvelopeRowButtonGap * 3.f));
            slider.setBounds(row);
        };

        placeMorphSlider(controls.firstSlider, controls.firstLabel, 1);
        placeMorphSlider(controls.secondSlider, controls.secondLabel, 2);

        const auto markerButtons = envelopeMarkerButtonLayout(bounds.toFloat());
        controls.firstButton.setBounds(markerButtons.loop);
        controls.secondButton.setBounds(markerButtons.sustain);
        controls.thirdButton.setBounds(markerButtons.log);
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

bool Effect2DExpandedEditorComponent::updateEnvelopeViewAxisFromPoint(Point<float> position) {
    const Rectangle<float> controlsArea = controlBounds().toFloat();

    for (int i = 0; i < 3; ++i) {
        if (envelopePrimaryAxisBounds(controlsArea, i).contains(position)) {
            if (envelopeViewAxis != i) {
                envelopeViewAxis = i;
                repaint();

                if (callbacks.repaintOpenGL != nullptr) {
                    callbacks.repaintOpenGL();
                }
            }
            return true;
        }

        if (i != 0 && envelopeLinkToggleBounds(controlsArea, i).contains(position)) {
            envelopeAxisLinked[i] = !envelopeAxisLinked[i];
            syncEnvelopeAxisLinksToWidget();
            repaint();

            if (callbacks.repaintOpenGL != nullptr) {
                callbacks.repaintOpenGL();
            }
            return true;
        }
    }

    return false;
}

bool Effect2DExpandedEditorComponent::beginVertexParameterEdit(Point<float> position) {
    String parameterId;
    float value {};

    if (!findVertexParameterAt(position, parameterId, value)) {
        return false;
    }

    activeVertexParameterId = parameterId;
    draggingVertexParameter = true;
    return widget.setSelectedVertexParameter(parameterId, value);
}

bool Effect2DExpandedEditorComponent::updateVertexParameterEdit(Point<float> position) {
    if (activeVertexParameterId.isEmpty()) {
        return false;
    }

    float value {};

    if (!findVertexParameterValueAt(activeVertexParameterId, position, value)) {
        return false;
    }

    const bool updated = widget.setSelectedVertexParameter(activeVertexParameterId, value);

    if (updated) {
        repaint();

        if (callbacks.repaintOpenGL != nullptr) {
            callbacks.repaintOpenGL();
        }
    }

    return updated;
}

bool Effect2DExpandedEditorComponent::showVertexParameterGuideMenu(Point<float> position) {
    String parameterId;
    Rectangle<int> targetBounds;

    if (!findVertexGuideAt(position, parameterId, targetBounds)) {
        return false;
    }

    PopupMenu menu;
    menu.addItem(1, "Guide attachments are available on mesh nodes", false, false);
    menu.showMenuAsync(PopupMenu::Options().withTargetScreenArea(targetBounds));
    return true;
}

bool Effect2DExpandedEditorComponent::findVertexParameterAt(
        Point<float> position,
        String& parameterId,
        float& value) const {
    if (node.kind != NodeKind::Envelope) {
        return false;
    }

    const Rectangle<float> parameterArea = envelopeVertexParameterBounds(controlBounds().toFloat());
    const auto parameters = widget.selectedVertexParameters();

    for (int i = 0; i < (int) parameters.size(); ++i) {
        const Rectangle<float> row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i);
        const Rectangle<float> rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(row);

        if (!rail.expanded(5.f, 8.f).contains(position)) {
            continue;
        }

        const auto& parameter = parameters[(size_t) i];
        parameterId = parameter.id;
        value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
        return true;
    }

    return false;
}

bool Effect2DExpandedEditorComponent::findVertexParameterValueAt(
        const String& parameterId,
        Point<float> position,
        float& value) const {
    if (node.kind != NodeKind::Envelope) {
        return false;
    }

    const Rectangle<float> parameterArea = envelopeVertexParameterBounds(controlBounds().toFloat());
    const auto parameters = widget.selectedVertexParameters();

    for (int i = 0; i < (int) parameters.size(); ++i) {
        if (parameters[(size_t) i].id != parameterId) {
            continue;
        }

        const Rectangle<float> row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i);
        const Rectangle<float> rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(row);
        value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
        return true;
    }

    return false;
}

bool Effect2DExpandedEditorComponent::findVertexGuideAt(
        Point<float> position,
        String& parameterId,
        Rectangle<int>& targetBounds) const {
    if (node.kind != NodeKind::Envelope) {
        return false;
    }

    const Rectangle<float> parameterArea = envelopeVertexParameterBounds(controlBounds().toFloat());
    const auto parameters = widget.selectedVertexParameters();

    for (int i = 0; i < (int) parameters.size(); ++i) {
        const Rectangle<float> row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i);
        const Rectangle<float> guide = TrimeshSidePanelRenderer::vertexParameterGuideBounds(row);

        if (!guide.expanded(4.f).contains(position)) {
            continue;
        }

        parameterId = parameters[(size_t) i].id;
        targetBounds = localAreaToGlobal(guide.toNearestInt());
        return true;
    }

    return false;
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

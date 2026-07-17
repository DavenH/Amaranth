#include "TransformCompactEditor.h"

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kLabelWidth = 78.f;

void drawModeChoice(
        Graphics& graphics,
        Rectangle<float> area,
        const String& label,
        const String& leftText,
        const String& rightText,
        bool rightActive,
        Colour colour) {
    Rectangle<float> labelArea = area.removeFromLeft(kLabelWidth);
    Rectangle<float> control = area.reduced(2.f, 0.f);
    const float corner = 6.f;
    const Rectangle<float> left = control.removeFromLeft(
            (control.getWidth() - 5.f) * 0.5f);
    control.removeFromLeft(5.f);
    const Rectangle<float> right = control;
    const Rectangle<float> active = rightActive ? right : left;

    graphics.setFont(FontOptions(11.f, Font::bold));
    graphics.setColour(kMutedText.withAlpha(0.76f));
    graphics.drawText(label, labelArea, Justification::centredLeft);
    graphics.setColour(Colour(0xff0e1318));
    graphics.fillRoundedRectangle(left, corner);
    graphics.fillRoundedRectangle(right, corner);
    graphics.setColour(kMutedText.withAlpha(0.32f));
    graphics.drawRoundedRectangle(left, corner, 1.f);
    graphics.drawRoundedRectangle(right, corner, 1.f);
    graphics.setColour(colour.withAlpha(0.18f));
    graphics.fillRoundedRectangle(active, corner);
    graphics.setColour(colour.withAlpha(0.82f));
    graphics.drawRoundedRectangle(active, corner, 1.2f);

    graphics.setFont(FontOptions(10.6f, Font::bold));
    graphics.setColour(rightActive
            ? kMutedText.withAlpha(0.62f)
            : kText.withAlpha(0.92f));
    graphics.drawText(leftText, left.reduced(5.f, 0.f), Justification::centred);
    graphics.setColour(rightActive
            ? kText.withAlpha(0.92f)
            : kMutedText.withAlpha(0.62f));
    graphics.drawText(rightText, right.reduced(5.f, 0.f), Justification::centred);
}

}

bool TransformCompactEditor::supports(NodeKind kind) {
    return kind == NodeKind::Fft || kind == NodeKind::Ifft;
}

Rectangle<float> TransformCompactEditor::expandedContentBounds(Rectangle<float> panel) {
    panel.removeFromTop(30.f);
    return panel.reduced(24.f, 18.f);
}

TransformMode TransformCompactEditor::mode(const Node& node) {
    if (node.kind == NodeKind::Fft) {
        return parameterValueForNode(node, "mode", "cycle") == "fixedWindow"
                ? TransformMode::FixedWindow
                : TransformMode::Cycle;
    }

    return parameterValueForNode(node, "mode", "cyclic") == "acyclicCarry"
            ? TransformMode::AcyclicCarry
            : TransformMode::Cyclic;
}

String TransformCompactEditor::parameterValue(TransformMode mode) {
    switch (mode) {
        case TransformMode::Cycle:         return "cycle";
        case TransformMode::FixedWindow:   return "fixedWindow";
        case TransformMode::Cyclic:        return "cyclic";
        case TransformMode::AcyclicCarry:  return "acyclicCarry";
    }

    return {};
}

String TransformCompactEditor::subtitle(NodeKind kind, TransformMode mode) {
    if (kind == NodeKind::Fft) {
        return mode == TransformMode::FixedWindow ? "fixed window" : "cycle chunks";
    }

    if (kind == NodeKind::Ifft) {
        return mode == TransformMode::AcyclicCarry ? "carry overlap" : "cyclic overlap";
    }

    return {};
}

String TransformCompactEditor::status(NodeKind kind, TransformMode mode) {
    if (kind == NodeKind::Fft) {
        return mode == TransformMode::FixedWindow
                ? "Time to freq: fixed time-window"
                : "Time to freq: chunked by cycle";
    }

    if (kind == NodeKind::Ifft) {
        return mode == TransformMode::AcyclicCarry
                ? "Freq to time: acyclic carry-buffer overlap"
                : "Freq to time: cyclic overlap";
    }

    return {};
}

void TransformCompactEditor::paint(
        Graphics& graphics,
        Rectangle<float> panel,
        const Node& node) {
    Rectangle<float> column = expandedContentBounds(panel);
    const TransformMode currentMode = mode(node);
    const Colour colour = colourForDomain(node.kind == NodeKind::Fft
            ? PortDomain::SpectralMagnitudeSignal
            : PortDomain::TimeSignal);

    if (node.kind == NodeKind::Fft) {
        drawModeChoice(
                graphics,
                column.removeFromTop(26.f),
                "Window",
                "Cycle chunks",
                "Fixed time",
                currentMode == TransformMode::FixedWindow,
                colour);
        return;
    }

    if (node.kind == NodeKind::Ifft) {
        drawModeChoice(
                graphics,
                column.removeFromTop(26.f),
                "Overlap",
                "Cyclic",
                "Acyclic carry",
                currentMode == TransformMode::AcyclicCarry,
                colour);
    }
}

std::optional<TransformMode> TransformCompactEditor::modeAt(
        const Node& node,
        Rectangle<float> panel,
        Point<float> position) {
    if (!supports(node.kind)) {
        return {};
    }

    Rectangle<float> column = expandedContentBounds(panel);
    Rectangle<float> control = column.removeFromTop(26.f)
            .withTrimmedLeft(kLabelWidth)
            .reduced(2.f, 0.f);
    Rectangle<float> left = control.removeFromLeft(
            (control.getWidth() - 5.f) * 0.5f);
    control.removeFromLeft(5.f);
    Rectangle<float> right = control;

    if (left.expanded(4.f).contains(position)) {
        return node.kind == NodeKind::Fft
                ? TransformMode::Cycle
                : TransformMode::Cyclic;
    }

    if (right.expanded(4.f).contains(position)) {
        return node.kind == NodeKind::Fft
                ? TransformMode::FixedWindow
                : TransformMode::AcyclicCarry;
    }

    return {};
}

}

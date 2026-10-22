#include "Nodes/Trimesh/Rendering/TrimeshSidePanelRenderer.h"

#include <cmath>

#include "Nodes/Trimesh/Rendering/OutputScaleControlRenderer.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"
#include "UI/Editors/PropertyControls.h"
#include "UI/NodeIconRenderer.h"
#include "UI/Editors/MorphControlLayout.h"
#include "UI/Editors/MorphSpaceLayout.h"
#include "UI/Editors/MeshEditorLayout.h"

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kColumnGap       = 12.f;
constexpr float kRangeRowHeight  = 45.f;
constexpr float kMorphColumnHeaderWidth = 24.f;
constexpr float kAxisLabelW      = 62.f;
constexpr float kVertexGainSize  = VertexPropertyMetrics::gainSize;
constexpr float kVertexGuideW    = VertexPropertyMetrics::guideWidth;
constexpr float kVertexCellGap   = VertexPropertyMetrics::cellGap;
constexpr float kWideLayoutMinimumWidth = 2.f * VertexPropertyMetrics::minimumColumnWidth + kColumnGap;
const int kCubeFaces[6][4] {
        { 0, 2, 3, 1 },
        { 4, 5, 7, 6 },
        { 0, 4, 5, 1 },
        { 2, 6, 7, 3 },
        { 0, 2, 6, 4 },
        { 3, 7, 6, 2 }
};

void paintGuideGainKnob(
        Graphics& g,
        Rectangle<float> bounds,
        float value,
        bool enabled) {
    const float normalized = jlimit(0.f, 1.f, value);
    const float startAngle = MathConstants<float>::pi * 1.25f;
    const float endAngle = MathConstants<float>::pi * 2.75f;
    const float valueAngle = startAngle + normalized * (endAngle - startAngle);
    const Point<float> centre = bounds.getCentre();
    const float radius = bounds.getWidth() * 0.5f - 2.f;

    g.setColour(Colour(0xff15191e).withAlpha(enabled ? 1.f : 0.55f));
    g.fillEllipse(bounds);
    g.setColour(Colour(0xff59606a).withAlpha(enabled ? 0.62f : 0.25f));
    g.drawEllipse(bounds.reduced(0.5f), 1.f);

    Path track;
    track.addCentredArc(
            centre.x,
            centre.y,
            radius - 2.f,
            radius - 2.f,
            0.f,
            startAngle,
            endAngle,
            true);
    g.setColour(Colour(0xff59606a).withAlpha(enabled ? 0.52f : 0.20f));
    g.strokePath(track, PathStrokeType(1.5f));

    Path active;
    active.addCentredArc(
            centre.x,
            centre.y,
            radius - 2.f,
            radius - 2.f,
            0.f,
            startAngle,
            valueAngle,
            true);
    g.setColour(Colour(0xff70a7ff).withAlpha(enabled ? 0.94f : 0.20f));
    g.strokePath(active, PathStrokeType(1.7f));

    const Point<float> indicator(
            centre.x + std::sin(valueAngle) * (radius - 4.f),
            centre.y - std::cos(valueAngle) * (radius - 4.f));
    g.drawLine(centre.x, centre.y, indicator.x, indicator.y, 1.4f);
}

MorphControlLayout controlLayout(Rectangle<float> area, bool showOutputScale) {
    if (showOutputScale) {
        area.removeFromBottom(MeshEditorLayout::controlFooterHeight);
    }
    return MorphControlLayout(area, 3, MorphDimensions::Plane::YellowRed);
}

Path cubeFacePath(const std::array<Point<float>, 8>& points, int faceIndex) {
    Path face;
    face.startNewSubPath(points[(size_t) kCubeFaces[faceIndex][0]]);
    face.lineTo(points[(size_t) kCubeFaces[faceIndex][1]]);
    face.lineTo(points[(size_t) kCubeFaces[faceIndex][2]]);
    face.lineTo(points[(size_t) kCubeFaces[faceIndex][3]]);
    face.closeSubPath();
    return face;
}

Rectangle<float> axisLabelBounds(Rectangle<float> row) {
    return row.removeFromLeft(kAxisLabelW).reduced(8.f, 0.f);
}

Rectangle<float> vertexGuideBounds(Rectangle<float> row) {
    row.removeFromRight(kVertexGainSize + kVertexCellGap);
    return row.removeFromRight(kVertexGuideW);
}

Rectangle<float> vertexLabelBounds(Rectangle<float> row) {
    return row.removeFromLeft(VertexPropertyMetrics::labelWidth).reduced(8.f, 0.f);
}

std::array<bool, 8> linkedCubeHighlights(
        const std::array<TrimeshSidePanelRenderer::AxisState, 3>& axes,
        const std::vector<TrimeshCubePreviewVertex>& cubeVertices) {
    std::array<bool, 8> selected {};
    int selectedIndex = -1;

    for (int i = 0; i < (int) jmin((size_t) 8, cubeVertices.size()); ++i) {
        if (cubeVertices[(size_t) i].selected) {
            selectedIndex = i;
            break;
        }
    }

    if (selectedIndex < 0) {
        return selected;
    }

    const bool linkYellow = axes[0].linked;
    const bool linkRed = axes[1].linked;
    const bool linkBlue = axes[2].linked;
    const int numLinks = (int) linkYellow + (int) linkRed + (int) linkBlue;
    selected[(size_t) selectedIndex] = true;

    if (numLinks == 3) {
        selected.fill(true);
    } else if (numLinks == 2) {
        if (!linkBlue) {
            const bool onYellowFace = selectedIndex == 0
                    || selectedIndex == 2
                    || selectedIndex == 4
                    || selectedIndex == 6;
            const int addend = onYellowFace ? 0 : 1;
            selected[(size_t) (0 + addend)] = true;
            selected[(size_t) (2 + addend)] = true;
            selected[(size_t) (4 + addend)] = true;
            selected[(size_t) (6 + addend)] = true;
        } else if (!linkRed) {
            const bool onRedFace = selectedIndex == 0
                    || selectedIndex == 1
                    || selectedIndex == 4
                    || selectedIndex == 5;
            const int addend = onRedFace ? 0 : 2;
            selected[(size_t) (0 + addend)] = true;
            selected[(size_t) (4 + addend)] = true;
            selected[(size_t) (5 + addend)] = true;
            selected[(size_t) (1 + addend)] = true;
        } else if (!linkYellow) {
            const bool onBlueFace = selectedIndex == 0
                    || selectedIndex == 1
                    || selectedIndex == 2
                    || selectedIndex == 3;
            const int addend = onBlueFace ? 0 : 4;
            selected[(size_t) (0 + addend)] = true;
            selected[(size_t) (1 + addend)] = true;
            selected[(size_t) (2 + addend)] = true;
            selected[(size_t) (3 + addend)] = true;
        }
    } else if (numLinks == 1) {
        int addend = 0;

        if (linkYellow) {
            addend = (selectedIndex == 0 || selectedIndex == 1 || selectedIndex == 2 || selectedIndex == 3) ? 4 : -4;
        } else if (linkRed) {
            addend = (selectedIndex == 0 || selectedIndex == 1 || selectedIndex == 4 || selectedIndex == 5) ? 2 : -2;
        } else if (linkBlue) {
            addend = (selectedIndex == 0 || selectedIndex == 2 || selectedIndex == 4 || selectedIndex == 6) ? 1 : -1;
        }

        selected[(size_t) (selectedIndex + addend)] = true;
    }

    return selected;
}

void drawAxisSlider(
        Graphics& g,
        Rectangle<float> row,
        Rectangle<float> rail,
        const TrimeshSidePanelRenderer::AxisState& axis) {
    const float value = jlimit(0.f, 1.f, axis.value);
    paintMorphSlider(g, rail, value, axis.colour);

    g.setColour(kText);
    g.setFont(FontOptions(12.f));
    g.drawText(axis.label, axisLabelBounds(row), Justification::centredLeft);
}


}

void TrimeshSidePanelRenderer::drawSidePanel(
        Graphics& g,
        Rectangle<float> area,
        const std::array<AxisState, 3>& axes,
        const std::vector<TrimeshCubePreviewVertex>& cubeVertices,
        const std::vector<TrimeshVertexParameter>& parameters,
        const std::array<String, 6>& guideAttachmentLabels,
        bool showOutputScale,
        PortDomain outputDomain,
        float outputScale,
        int dimensionCount, const String& feedbackParameter) {
    auto controls = area;
    if (showOutputScale) {
        controls.removeFromBottom(MeshEditorLayout::controlFooterHeight);
    }
    drawMorphControls(g, controls, axes, dimensionCount, MorphDimensions::Plane::YellowRed, cubeVertices);
    drawVertexParameters(g, MorphControlLayout(controls, dimensionCount).vertices(),
            parameters, guideAttachmentLabels, 1.f, GuideControls::Visible, 7, feedbackParameter);
    if (showOutputScale) {
        const auto outputRow = outputScaleRowBounds(area);
        paintPropertyGroupLabel(g, outputRow.withY(outputRow.getY() - 10.f).withHeight(12.f), "Output");
        OutputScaleControlRenderer::draw(g, outputRow,
                outputScaleRailBounds(area), outputDomain, outputScale);
    }
}

void TrimeshSidePanelRenderer::drawMorphControls(
        Graphics& g, Rectangle<float> area,
        const std::array<AxisState, 3>& axes, int dimensionCount,
        MorphDimensions::Plane plane, const std::vector<TrimeshCubePreviewVertex>& vertices) {
    const MorphControlLayout layout(area, dimensionCount, plane);
    if (dimensionCount == 3) {
        drawMorphCubePreview(g, layout.diagram(), axes, vertices);
    } else if (dimensionCount > 0) {
        drawReducedMorphPreview(g, layout.diagram(), axes, MorphDimensions(dimensionCount, plane), vertices);
    }
    const MorphSpaceLayout space(layout.diagram(), MorphDimensions(dimensionCount, plane));
    if (dimensionCount > 0) {
        paintPropertyGroupLabel(g, layout.diagram().reduced(8.f, 5.f).removeFromTop(18.f), "Morph space");
    }
    for (int index = 0; index < 3; ++index) {
        const auto target = space.locks[(size_t) index];
        if (!target.isEmpty()) {
            const auto& state = axes[(size_t) index];
            const auto rail = space.rails[(size_t) index];
            g.setColour(state.colour.withAlpha(0.8f));
            g.drawLine(rail, 3.2f);
            g.setColour(state.colour.withAlpha(0.45f));
            g.drawLine(Line<float>(rail.getPointAlongLineProportionally(0.5f), target.getCentre()), 1.f);
            paintMorphAxisLock(g, target, state.colour, state.linked);
            g.setColour(state.colour);
            g.setFont(FontOptions(9.f));
            g.drawText(state.shortLabel, space.labels[(size_t) index], Justification::centred);
        }
        const auto row = layout.row(index);
        if (row.isEmpty()) {
            continue;
        }
        const auto& state = axes[(size_t) index];
        const int firstAxis = dimensionCount == 2 && plane == MorphDimensions::Plane::RedBlue ? 1 : 0;
        if (index == firstAxis) {
            drawMorphColumnHeaders(g, row, layout.axis(index));
        }
        drawAxisSlider(g, row, layout.rail(index), state);
        const auto axis = layout.axis(index);
        if (!axis.isEmpty()) {
            g.setColour(state.colour.withAlpha(state.primary ? 0.45f : 0.1f));
            g.fillRoundedRectangle(axis, 3.f);
            g.setColour(state.colour.withAlpha(state.primary ? 1.f : 0.4f));
            g.drawRoundedRectangle(axis, 3.f, 1.f);
            g.setFont(FontOptions(10.f).withStyle(state.primary ? "Bold" : "Regular"));
            g.drawText(state.shortLabel, axis, Justification::centred);
        }
    }
}

void TrimeshSidePanelRenderer::drawMorphCubePreview(
        Graphics& g,
        Rectangle<float> area,
        const std::array<AxisState, 3>& axes,
        const std::vector<TrimeshCubePreviewVertex>& cubeVertices) {
    if (area.getWidth() < 42.f || area.getHeight() < 42.f) {
        return;
    }

    const MorphSpaceLayout space(area, MorphDimensions(3));
    const auto& unitPoints = space.corners;
    std::array<Point<float>, 8> previewPoints;
    const bool hasPreviewCube = cubeVertices.size() >= 8;
    for (int i = 0; i < 8; ++i) {
        previewPoints[(size_t) i] = hasPreviewCube
                ? space.project(cubeVertices[(size_t) i].red,
                        cubeVertices[(size_t) i].blue, cubeVertices[(size_t) i].time)
                : unitPoints[(size_t) i];
    }

    const Colour axisColours[3] {
            Colour(0.65f, 0.65f, 0.64f, 1.f),
            Colour(0.95f, 0.65f, 0.40f, 1.f),
            Colour(0.11f, 0.65f, 0.45f, 1.f)
    };

    for (int i = 0; i < 6; ++i) {
        Path face = cubeFacePath(unitPoints, i);
        const Point<float> start = unitPoints[(size_t) kCubeFaces[i][0]];
        const Point<float> end = unitPoints[(size_t) kCubeFaces[i][2]];

        g.setGradientFill(ColourGradient(
                Colour::greyLevel(0.25f),
                start.x,
                start.y,
                Colour::greyLevel(0.25f).withAlpha(0.f),
                end.x,
                end.y,
                true));
        g.fillPath(face);
    }

    if (hasPreviewCube) {
        for (int i = 0; i < 6; ++i) {
            Path face = cubeFacePath(previewPoints, i);
            const int colourIndex = (i == 0 || i == 1) ? 1 : (i == 2 || i == 3) ? 0 : 2;
            const Point<float> start = previewPoints[(size_t) kCubeFaces[i][1]];
            const Point<float> end = previewPoints[(size_t) kCubeFaces[i][3]];

            g.setGradientFill(ColourGradient(
                    axisColours[colourIndex].withAlpha(0.7f),
                    start.x,
                    start.y,
                    axisColours[colourIndex].withAlpha(0.3f),
                    end.x,
                    end.y,
                    true));
            g.fillPath(face);
        }
    }

    const float red = jlimit(0.f, 1.f, axes[1].value);
    const float blue = jlimit(0.f, 1.f, axes[2].value);
    const float time = jlimit(0.f, 1.f, axes[0].value);
    const Point<float> morphPoint = space.project(red, blue, time);
    const Point<float> timeProjection = space.project(red, blue, 0.f);
    const Point<float> redProjection = space.project(0.f, blue, time);
    const Point<float> blueProjection = space.project(red, 0.f, time);
    const Colour cursorColours[3] {
            Colour(0xffe0c247),
            Colour(0xffd65a5a),
            Colour(0xff5f91e8)
    };
    const Point<float> projections[3] {
            timeProjection,
            redProjection,
            blueProjection
    };

    for (int i = 0; i < 3; ++i) {
        g.setColour(cursorColours[i].withAlpha(0.26f));
        g.drawLine(
                morphPoint.x,
                morphPoint.y,
                projections[i].x,
                projections[i].y,
                1.1f);
    }

    const auto linkedHighlights = linkedCubeHighlights(axes, cubeVertices);

    for (int i = 0; i < 8; ++i) {
        const bool selected = hasPreviewCube && linkedHighlights[(size_t) i];

        paintMorphPreviewVertex(g, previewPoints[(size_t) i], selected);
    }

    paintMorphPreviewCursor(g, morphPoint);
}

int TrimeshSidePanelRenderer::vertexParameterSlot(const String& id, int slotCount) {
    if (id == "guide.time") {
        return slotCount - 1;
    }
    const String field = id.fromLastOccurrenceOf(".", false, false);
    const int index = field == "time" ? 0 : field == "red" ? 1 : field == "blue" ? 2
            : field == "phase" ? 3 : field == "amp" ? 4 : 5;
    return index;
}

void TrimeshSidePanelRenderer::drawVertexParameters(
        Graphics& g,
        Rectangle<float> area,
        const std::vector<TrimeshVertexParameter>& parameters,
        const std::array<String, 6>& guideAttachmentLabels,
        float heightScale,
        GuideControls guideControls,
        int stableSlotCount,
        const String& feedbackParameter) {
    if (area.getHeight() < 28.f) {
        return;
    }

    const int slotCount = stableSlotCount > 0 ? stableSlotCount : (int) parameters.size();
    const int rowCount = area.getWidth() >= kWideLayoutMinimumWidth
            ? (slotCount + 1) / 2 : jmax(1, slotCount);
    const float desiredHeight = heightScale * (34.f + (float) rowCount * 40.f);
    area = area.withHeight(jmin(area.getHeight(), desiredHeight));

    auto header = area.reduced(8.f, 5.f).removeFromTop(18.f);
    const auto headingRow = vertexParameterRowBounds(area, 0, heightScale, slotCount);
    paintPropertyGroupLabel(g, header.withRight(vertexGuideBounds(headingRow).getX() - kVertexCellGap), "Vertex");
    g.setColour(kMutedText);
    g.setFont(FontOptions(9.f));
    g.drawText("Guide", vertexGuideBounds(headingRow).withY(header.getY()).withHeight(header.getHeight()), Justification::centred);
    g.drawText("Gain", vertexParameterGuideGainBounds(headingRow).withY(header.getY()).withHeight(header.getHeight()), Justification::centred);

    for (int i = 0; i < (int) parameters.size(); ++i) {
        const auto& parameter = parameters[(size_t) i];
        const int slot = stableSlotCount > 0 ? vertexParameterSlot(parameter.id, slotCount) : i;
        const auto row = vertexParameterRowBounds(area, slot, heightScale, slotCount);
        drawVertexParameter(g, row, parameter, guideAttachmentLabels, guideControls, feedbackParameter);
    }
}

void TrimeshSidePanelRenderer::drawVertexParameter(
        Graphics& g, Rectangle<float> row, const TrimeshVertexParameter& parameter,
        const std::array<String, 6>& guideAttachmentLabels, GuideControls guideControls,
        const String& feedbackParameter) {
    const bool showRowGuideControls = guideControls == GuideControls::Visible
            && showsGuideControlsForParameter(parameter.id);
    const auto rail = vertexParameterRailBounds(row, guideControls);

    const auto guideBox = vertexGuideBounds(row);
    auto labelBox = parameter.guideOnly
            ? row.withRight(guideBox.getX() - kVertexCellGap).reduced(8.f, 0.f) : vertexLabelBounds(row);
    const auto gainBox = vertexParameterGuideGainBounds(row);
    if (parameter.guideOnly) {
        const Rectangle<float> icon(labelBox.getRight() - 64.f, row.getCentreY() - 10.f, 64.f, 20.f);
        NodeIconRenderer::paint(g, "componentCurve", icon);
        labelBox.setRight(icon.getX() - 8.f);
    }

    g.setColour(parameter.enabled ? Colour(0xffb8c1cd) : Colour(0xff8793a1));
    g.setFont(FontOptions(11.f));
    g.drawText(parameter.label.toLowerCase(), labelBox, Justification::centredLeft);

    const float range = parameter.maximum - parameter.minimum;
    const float normalized = range > 0.f
            ? jlimit(0.f, 1.f, (parameter.value - parameter.minimum) / range)
            : 0.f;

    if (!parameter.guideOnly) {
        paintMorphSlider(g, rail, normalized, Colour(0xffb7bec7),
                feedbackParameter == parameter.id, parameter.enabled);
    }

    const String field = parameter.id.fromLastOccurrenceOf(".", false, false);
    if (parameter.enabled && ((!parameter.guideOnly && feedbackParameter == parameter.id)
            || (showRowGuideControls && feedbackParameter == "guideGain." + field))) {
        const bool gain = feedbackParameter.startsWith("guideGain.");
        const String text = gain ? String(parameter.guideGain * 100.f, 1) + "%" : String(parameter.value, 3);
        g.setColour(kText);
        g.setFont(FontOptions(10.5f));
        g.drawText(text, rail.withY(row.getY()).withHeight(12.f), Justification::centredRight);
    }
    if (!showRowGuideControls) {
        return;
    }

    const int guideIndex = field == "time" ? 0 : field == "red" ? 1
            : field == "blue" ? 2 : field == "phase" ? 3 : field == "amp" ? 4 : 5;
    const String guideLabel = guideAttachmentLabels[(size_t) guideIndex];
    Graphics::ScopedSaveState guideState(g);
    if (!parameter.enabled) {
        g.beginTransparencyLayer(0.45f);
    }
    paintGuideGainKnob(g, gainBox, parameter.guideGain, parameter.enabled && guideLabel.isNotEmpty());
    g.setColour(guideLabel.isEmpty() ? Colour(0xff15191e) : Colour(0xff202833));
    g.fillRect(guideBox);
    g.setColour(guideLabel.isEmpty()
            ? Colour(0xff59606a).withAlpha(0.45f)
            : Colour(0xff70a7ff).withAlpha(0.72f));
    g.drawRect(guideBox, 1.f);

    if (guideLabel.isNotEmpty()) {
        g.setColour(Colour(0xff70a7ff).withAlpha(0.90f));
        g.setFont(FontOptions(10.5f));
        g.drawText(guideLabel, guideBox.reduced(3.f, 0.f), Justification::centred);
    } else {
        NodeIconRenderer::paint(
                g,
                "guideCurve",
                guideBox.reduced(8.f, 1.f).withTrimmedRight(3.f),
                0.82f);
    }

    Path chevron;
    chevron.startNewSubPath(guideBox.getRight() - 8.f, guideBox.getCentreY() - 2.f);
    chevron.lineTo(guideBox.getRight() - 5.f, guideBox.getCentreY() + 2.f);
    chevron.lineTo(guideBox.getRight() - 2.f, guideBox.getCentreY() - 2.f);
    g.strokePath(chevron, PathStrokeType(1.1f));
    if (!parameter.enabled) {
        g.endTransparencyLayer();
    }
}

void TrimeshSidePanelRenderer::drawMorphColumnHeaders(
        Graphics& g,
        Rectangle<float> firstRow,
        Rectangle<float> axisButton) {
    if (!axisButton.isEmpty()) {
        paintPropertyGroupLabel(g, morphColumnHeaderBounds(axisButton, firstRow), "Axis");
    }
}

Rectangle<float> TrimeshSidePanelRenderer::morphCubeBounds(
        Rectangle<float> sideArea,
        bool showOutputScale) {
    return controlLayout(sideArea, showOutputScale).diagram();
}

Rectangle<float> TrimeshSidePanelRenderer::morphRailBounds(
        Rectangle<float> sideArea,
        int axisIndex,
        bool showOutputScale) {
    return controlLayout(sideArea, showOutputScale).rail(axisIndex);
}

Rectangle<float> TrimeshSidePanelRenderer::morphMarkerBounds(
        Rectangle<float> rail,
        float value) {
    return morphSliderIndicatorBounds(rail, value);
}

Rectangle<float> TrimeshSidePanelRenderer::morphColumnHeaderBounds(
        Rectangle<float> button,
        Rectangle<float> firstRow) {
    return Rectangle<float>(kMorphColumnHeaderWidth, 10.f)
            .withCentre({ button.getCentreX(), firstRow.getY() - 7.f });
}

Rectangle<float> TrimeshSidePanelRenderer::primaryAxisBounds(
        Rectangle<float> sideArea,
        int axisIndex,
        bool showOutputScale) {
    return controlLayout(sideArea, showOutputScale).axis(axisIndex);
}

Rectangle<float> TrimeshSidePanelRenderer::linkToggleBounds(
        Rectangle<float> sideArea,
        int axisIndex,
        bool showOutputScale) {
    return controlLayout(sideArea, showOutputScale).link(axisIndex);
}

Rectangle<float> TrimeshSidePanelRenderer::keyboardFocusBounds(
        Rectangle<float> sideArea,
        int axisIndex,
        bool showOutputScale,
        int dimensionCount) {
    auto controls = sideArea;
    if (showOutputScale) {
        controls.removeFromBottom(MeshEditorLayout::controlFooterHeight);
    }
    const MorphControlLayout layout(controls, dimensionCount, MorphDimensions::Plane::YellowRed);
    return layout.keyboardFocus(axisIndex);
}

Rectangle<float> TrimeshSidePanelRenderer::outputScaleRowBounds(Rectangle<float> sideArea, int) {
    const auto column = controlLayout(sideArea, true).vertices().reduced(VertexPropertyMetrics::columnInset, 0.f);
    return column.withY(sideArea.getBottom() - kRangeRowHeight).withHeight(kRangeRowHeight);
}

Rectangle<float> TrimeshSidePanelRenderer::outputScaleRailBounds(Rectangle<float> sideArea, int dimensionCount) {
    auto row = outputScaleRowBounds(sideArea, dimensionCount);
    row.removeFromLeft(VertexPropertyMetrics::labelWidth + VertexPropertyMetrics::cellGap);
    row.removeFromRight(8.f);
    return {
            row.getX(),
            row.getY() + 9.f,
            jmax(36.f, row.getWidth()),
            7.f
    };
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterPanelBounds(
        Rectangle<float> sideArea,
        bool showOutputScale) {
    return controlLayout(sideArea, showOutputScale).vertices();
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterRowBounds(
        Rectangle<float> parameterArea,
        int parameterIndex,
        float heightScale,
        int parameterCount) {
    auto rows = parameterArea.reduced(VertexPropertyMetrics::columnInset, 0.f);
    const float headerHeight = 34.f * heightScale;
    const float gap = 7.f * heightScale;
    const bool useTwoColumns = parameterArea.getWidth() >= kWideLayoutMinimumWidth;
    const int rowCount = useTwoColumns ? (parameterCount + 1) / 2 : jmax(1, parameterCount);
    const float rowHeight = jmin(
            34.f * heightScale,
            jmax(0.f, (rows.getHeight() - headerHeight - gap * (float) (rowCount - 1)) / rowCount));

    rows.removeFromTop(headerHeight);
    const int columnIndex = useTwoColumns && parameterIndex >= rowCount ? 1 : 0;
    const int rowIndex = useTwoColumns ? parameterIndex % rowCount : parameterIndex;

    if (useTwoColumns) {
        const float columnWidth = (rows.getWidth() - kColumnGap) * 0.5f;
        rows = rows.withWidth(columnWidth);
        rows.translate((columnWidth + kColumnGap) * (float) columnIndex, 0.f);
    }

    rows.translate(0.f, (float) rowIndex * (rowHeight + gap));
    return rows.removeFromTop(rowHeight);
}

String TrimeshSidePanelRenderer::propertyTooltip(
        const std::vector<TrimeshVertexParameter>& parameters, const String& id) {
    for (const auto& parameter : parameters) {
        const bool gain = id == "guideGain." + parameter.id.fromLastOccurrenceOf(".", false, false)
                && showsGuideControlsForParameter(parameter.id);
        if (parameter.id != id && !gain) {
            continue;
        }
        if (!parameter.enabled) {
            return parameter.label + ": unavailable for the current morph mode or selection.";
        }
        if (parameter.guideOnly && !gain) {
            return "Choose a Guide curve for the component between intercepts.";
        }
        return parameter.label + (gain ? " Guide gain: " + String(parameter.guideGain * 100.f, 1) + "%"
                : ": " + String(parameter.value, 3));
    }
    return {};
}

bool TrimeshSidePanelRenderer::showsGuideControlsForParameter(const String& parameterId) {
    return parameterId != "vertex.time";
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterRailBounds(
        Rectangle<float> parameterRow,
        GuideControls guideControls) {
    parameterRow.removeFromLeft(VertexPropertyMetrics::labelWidth + kVertexCellGap);
    if (guideControls == GuideControls::Visible) {
        parameterRow.removeFromRight(
                kVertexGuideW
                + kVertexCellGap
                + kVertexGainSize
                + kVertexCellGap);
        return parameterRow.withSizeKeepingCentre(
                jmax(4.f, parameterRow.getWidth()),
                8.f);
    }
    return parameterRow.withTrimmedRight(8.f)
            .withSizeKeepingCentre(jmax(4.f, parameterRow.getWidth() - 8.f), 8.f);
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterGuideGainBounds(
        Rectangle<float> parameterRow) {
    return parameterRow.removeFromRight(kVertexGainSize)
            .withSizeKeepingCentre(kVertexGainSize, kVertexGainSize);
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterGuideBounds(Rectangle<float> parameterRow) {
    return vertexGuideBounds(parameterRow);
}

}

#include "TrimeshSidePanelRenderer.h"

#include <limits>

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kSideInset       = 9.f;
constexpr float kMorphHeaderH    = 18.f;
constexpr float kMorphRowHeight  = 31.f;
constexpr float kAxisButtonSize  = 20.f;
constexpr float kRowButtonGap    = 6.f;
constexpr float kVertexGap       = 10.f;
constexpr float kColumnGap       = 12.f;
constexpr float kMorphTopGap     = 10.f;
constexpr float kAxisLabelW      = 62.f;
constexpr float kVertexLabelW    = 58.f;
constexpr float kVertexGuideW    = 46.f;
constexpr float kVertexCellGap   = 8.f;
constexpr float kCubeAxisExpansion = 0.09f;
constexpr int kVertexParamCount  = 6;

const float kCubeProjection[4][2] {
        { 0.525f,  0.300f },
        { 0.000f,  0.675f },
        { 0.675f, -0.225f },
        { 0.188f,  0.300f }
};

const float kCubeCorners[8][3] {
        { 0.f, 0.f, 0.f },
        { 1.f, 0.f, 0.f },
        { 0.f, 1.f, 0.f },
        { 1.f, 1.f, 0.f },
        { 0.f, 0.f, 1.f },
        { 1.f, 0.f, 1.f },
        { 0.f, 1.f, 1.f },
        { 1.f, 1.f, 1.f }
};

const int kCubeFaces[6][4] {
        { 0, 2, 3, 1 },
        { 4, 5, 7, 6 },
        { 0, 4, 5, 1 },
        { 2, 6, 7, 3 },
        { 0, 2, 6, 4 },
        { 3, 7, 6, 2 }
};

const int kCubeAxes[3][2] {
        { 0, 2 },
        { 0, 1 },
        { 1, 5 }
};

Rectangle<float> sideInnerBounds(Rectangle<float> sideArea) {
    return sideArea.reduced(kSideInset, 0.f);
}

float upperPanelHeight(Rectangle<float> sideArea) {
    const auto inner = sideInnerBounds(sideArea);
    return jlimit(150.f, 240.f, inner.getHeight() * 0.58f);
}

Rectangle<float> upperPanelBounds(Rectangle<float> sideArea) {
    auto inner = sideInnerBounds(sideArea);
    return inner.removeFromTop(upperPanelHeight(sideArea));
}

Rectangle<float> cubeStackBounds(Rectangle<float> sideArea) {
    auto upper = upperPanelBounds(sideArea);
    const float width = jmax(80.f, (upper.getWidth() - kColumnGap) * 0.5f);
    return upper.removeFromLeft(width);
}

Rectangle<float> vertexPanelColumnBounds(Rectangle<float> sideArea) {
    auto upper = upperPanelBounds(sideArea);
    upper.removeFromLeft(cubeStackBounds(sideArea).getWidth() + kColumnGap);
    return upper;
}

Rectangle<float> morphControlsBounds(Rectangle<float> sideArea) {
    auto inner = sideInnerBounds(sideArea);
    inner.removeFromTop(upperPanelHeight(sideArea) + kVertexGap);
    return inner;
}

Point<float> projectCubeUnit(float red, float blue, float time) {
    return {
            kCubeProjection[0][0] * red
                    + kCubeProjection[1][0] * (1.f - blue)
                    + kCubeProjection[2][0] * time
                    + kCubeProjection[3][0],
            kCubeProjection[0][1] * red
                    + kCubeProjection[1][1] * (1.f - blue)
                    + kCubeProjection[2][1] * time
                    + kCubeProjection[3][1]
    };
}

Point<float> scaleCubePoint(Point<float> point, Rectangle<float> target, float size) {
    const Point<float> origin(
            target.getCentreX() - size * 0.80f,
            target.getCentreY() - size * 0.38f);

    return origin + point * size;
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

Rectangle<float> axisRowBounds(Rectangle<float> sideArea, int axisIndex) {
    auto controls = morphControlsBounds(sideArea);
    controls.removeFromTop(kMorphHeaderH + kMorphTopGap);

    return {
            controls.getX(),
            controls.getY() + (float) axisIndex * (kMorphRowHeight + 5.f),
            controls.getWidth(),
            kMorphRowHeight
    };
}

void drawScanLines(Graphics& g, Rectangle<float> area, Colour colour, float alpha) {
    g.setColour(colour.withAlpha(alpha));

    for (float y = area.getY() + 2.f; y < area.getBottom(); y += 3.f) {
        g.drawHorizontalLine(roundToInt(y), area.getX(), area.getRight());
    }
}

void drawSliderRowBody(Graphics& g, Rectangle<float> body) {
    g.setColour(Colour(0xff050607).withAlpha(0.86f));
    g.fillRect(body);
    drawScanLines(g, body, Colour(0xff59606a), 0.16f);
    g.setColour(Colour(0xffd8d5ca).withAlpha(0.22f));
    g.drawRect(body, 1.f);
}

Rectangle<float> axisLabelBounds(Rectangle<float> row) {
    return row.removeFromLeft(kAxisLabelW).reduced(8.f, 0.f);
}

Rectangle<float> vertexGuideBounds(Rectangle<float> row) {
    row.removeFromRight(kVertexCellGap);
    return row.removeFromRight(kVertexGuideW).reduced(0.f, 5.f);
}

Rectangle<float> vertexLabelBounds(Rectangle<float> row) {
    return row.removeFromLeft(kVertexLabelW).reduced(8.f, 0.f);
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

void drawMorphColumnHeaders(Graphics& g, Rectangle<float> sideArea) {
    const Rectangle<float> primary = TrimeshSidePanelRenderer::primaryAxisBounds(sideArea, 0);
    const Rectangle<float> link = TrimeshSidePanelRenderer::linkToggleBounds(sideArea, 0);
    const float y = axisRowBounds(sideArea, 0).getY() - 13.f;

    g.setColour(kMutedText.withAlpha(0.78f));
    g.setFont(FontOptions(8.5f, Font::bold));
    g.drawText("axis", primary.withY(y).withHeight(10.f), Justification::centred);
    g.drawText("link", link.withY(y).withHeight(10.f), Justification::centred);
}

void drawAxisSlider(
        Graphics& g,
        Rectangle<float> row,
        Rectangle<float> rail,
        const TrimeshSidePanelRenderer::AxisState& axis) {
    const Rectangle<float> sliderBody = row.withRight(rail.getRight() + 6.f);

    drawSliderRowBody(g, sliderBody);

    const float value = jlimit(0.f, 1.f, axis.value);
    const Rectangle<float> filled = rail.withWidth(rail.getWidth() * value);
    const float knobX = rail.getX() + rail.getWidth() * value;

    g.setColour(axis.colour.withAlpha(0.18f));
    g.fillRect(rail.withWidth(rail.getWidth() * value).expanded(0.f, 8.f));
    g.setColour(axis.colour.withAlpha(0.76f));
    g.fillRect(filled);

    g.setColour(kText);
    g.setFont(FontOptions(12.f, Font::bold));
    g.drawText(axis.label, axisLabelBounds(row), Justification::centredLeft);

    g.setColour(Colour(0xff202328).withAlpha(0.88f));
    g.fillEllipse(Rectangle<float>(8.f, 8.f).withCentre({ knobX, rail.getCentreY() }));
    g.setColour(axis.colour.withAlpha(0.88f));
    g.drawEllipse(Rectangle<float>(8.f, 8.f).withCentre({ knobX, rail.getCentreY() }), 1.4f);
    g.setColour(Colour(0xff05070a).withAlpha(0.72f));
    g.drawVerticalLine(roundToInt(knobX), rail.getY() - 5.f, rail.getBottom() + 5.f);
}

void drawPrimaryAxisButtons(
        Graphics& g,
        Rectangle<float> sideArea,
        const std::array<TrimeshSidePanelRenderer::AxisState, 3>& axes) {
    for (int i = 0; i < (int) axes.size(); ++i) {
        const auto& axis = axes[(size_t) i];
        const Rectangle<float> button = TrimeshSidePanelRenderer::primaryAxisBounds(sideArea, i);

        g.setColour(axis.colour.withAlpha(axis.primary ? 0.42f : 0.055f));
        g.fillRoundedRectangle(button, 4.f);
        g.setColour(axis.colour.withAlpha(axis.primary ? 1.f : 0.32f));
        g.drawRoundedRectangle(button, 4.f, axis.primary ? 1.8f : 1.f);

        if (axis.primary) {
            g.setColour(axis.colour.withAlpha(0.95f));
            g.fillRoundedRectangle(button.reduced(6.f, 6.f), 2.f);
        }
    }
}

void drawLinkRow(
        Graphics& g,
        Rectangle<float> sideArea,
        const std::array<TrimeshSidePanelRenderer::AxisState, 3>& axes) {
    for (int i = 0; i < (int) axes.size(); ++i) {
        const auto& axis = axes[(size_t) i];
        const Rectangle<float> toggle = TrimeshSidePanelRenderer::linkToggleBounds(sideArea, i);

        g.setColour(axis.colour.withAlpha(axis.linked ? 0.38f : 0.055f));
        g.fillRoundedRectangle(toggle, 4.f);
        g.setColour(axis.colour.withAlpha(axis.linked ? 0.96f : 0.32f));
        g.drawRoundedRectangle(toggle, 4.f, axis.linked ? 1.8f : 1.f);

        if (axis.linked) {
            g.setColour(axis.colour.withAlpha(0.88f));
            g.drawLine(
                    toggle.getX() + 6.f,
                    toggle.getCentreY(),
                    toggle.getRight() - 6.f,
                    toggle.getCentreY(),
                    1.5f);
        }
    }
}

}

void TrimeshSidePanelRenderer::drawSidePanel(
        Graphics& g,
        Rectangle<float> area,
        const std::array<AxisState, 3>& axes,
        const std::vector<TrimeshCubePreviewVertex>& cubeVertices,
        const std::vector<TrimeshVertexParameter>& parameters,
        const std::array<String, 6>& guideAttachmentLabels) {
    auto morphControls = morphControlsBounds(area);

    drawMorphCubePreview(g, morphCubeBounds(area), axes, cubeVertices);

    g.setColour(kMutedText);
    g.setFont(FontOptions(10.5f, Font::bold));
    g.drawText("morph Position", morphControls.removeFromTop(kMorphHeaderH), Justification::centred);

    drawMorphColumnHeaders(g, area);

    for (int i = 0; i < (int) axes.size(); ++i) {
        drawAxisSlider(g, axisRowBounds(area, i), morphRailBounds(area, i), axes[(size_t) i]);
    }

    drawPrimaryAxisButtons(g, area, axes);
    drawLinkRow(g, area, axes);
    drawVertexParameters(g, vertexParameterPanelBounds(area), parameters, guideAttachmentLabels);
}

void TrimeshSidePanelRenderer::drawMorphCubePreview(
        Graphics& g,
        Rectangle<float> area,
        const std::array<AxisState, 3>& axes,
        const std::vector<TrimeshCubePreviewVertex>& cubeVertices) {
    if (area.getWidth() < 42.f || area.getHeight() < 42.f) {
        return;
    }

    auto header = area.reduced(8.f, 5.f).removeFromTop(18.f);
    g.setColour(kMutedText);
    g.setFont(FontOptions(10.5f, Font::bold));
    g.drawText("cube display", header, Justification::centred);

    area = area.withTrimmedTop(22.f).reduced(3.f, 0.f);

    const auto expandedUnitPoint = [](int index, float expansion) {
        const float halfExpansion = expansion * 0.5f;
        return projectCubeUnit(
                (1.f + expansion) * kCubeCorners[index][0] - halfExpansion,
                (1.f + expansion) * kCubeCorners[index][1] - halfExpansion,
                (1.f + expansion) * kCubeCorners[index][2] - halfExpansion);
    };

    std::array<Point<float>, 8> rawExpandedPoints;
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (int i = 0; i < 8; ++i) {
        rawExpandedPoints[(size_t) i] = expandedUnitPoint(i, kCubeAxisExpansion);
        minX = jmin(minX, rawExpandedPoints[(size_t) i].x);
        minY = jmin(minY, rawExpandedPoints[(size_t) i].y);
        maxX = jmax(maxX, rawExpandedPoints[(size_t) i].x);
        maxY = jmax(maxY, rawExpandedPoints[(size_t) i].y);
    }

    const float rawWidth = jmax(0.001f, maxX - minX);
    const float rawHeight = jmax(0.001f, maxY - minY);
    const float size = jmin(area.getWidth() / rawWidth, area.getHeight() / rawHeight);
    const Point<float> origin(
            area.getCentreX() - (minX + rawWidth * 0.5f) * size,
            area.getCentreY() - (minY + rawHeight * 0.5f) * size);
    const auto fitPoint = [origin, size](Point<float> point) {
        return origin + point * size;
    };

    std::array<Point<float>, 8> unitPoints;
    std::array<Point<float>, 8> expandedPoints;
    std::array<Point<float>, 8> previewPoints;
    const bool hasPreviewCube = cubeVertices.size() >= 8;

    for (int i = 0; i < 8; ++i) {
        unitPoints[(size_t) i] = fitPoint(expandedUnitPoint(i, 0.f));
        expandedPoints[(size_t) i] = fitPoint(rawExpandedPoints[(size_t) i]);
        previewPoints[(size_t) i] = hasPreviewCube
                ? fitPoint(projectCubeUnit(
                        cubeVertices[(size_t) i].red,
                        cubeVertices[(size_t) i].blue,
                        cubeVertices[(size_t) i].time))
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

    for (int i = 0; i < 3; ++i) {
        g.setColour(axisColours[i]);
        g.drawLine(
                expandedPoints[(size_t) kCubeAxes[i][0]].x,
                expandedPoints[(size_t) kCubeAxes[i][0]].y,
                expandedPoints[(size_t) kCubeAxes[i][1]].x,
                expandedPoints[(size_t) kCubeAxes[i][1]].y,
                3.2f);
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
    const Point<float> morphPoint = fitPoint(projectCubeUnit(red, blue, time));
    const Point<float> timeProjection = fitPoint(projectCubeUnit(red, blue, 0.f));
    const Point<float> redProjection = fitPoint(projectCubeUnit(0.f, blue, time));
    const Point<float> blueProjection = fitPoint(projectCubeUnit(red, 0.f, time));
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

        g.setColour(Colours::black);
        g.fillEllipse(Rectangle<float>(selected ? 5.f : 4.f, selected ? 5.f : 4.f)
                .withCentre(previewPoints[(size_t) i]));
        g.setColour(selected ? Colours::red : Colours::white);
        g.fillEllipse(Rectangle<float>(selected ? 2.2f : 1.5f, selected ? 2.2f : 1.5f)
                .withCentre(previewPoints[(size_t) i]));
    }

    g.setColour(Colours::white);
    g.fillEllipse(Rectangle<float>(2.f, 2.f).withCentre(morphPoint));
}

void TrimeshSidePanelRenderer::drawVertexParameters(
        Graphics& g,
        Rectangle<float> area,
        const std::vector<TrimeshVertexParameter>& parameters,
        const std::array<String, 6>& guideAttachmentLabels) {
    if (area.getHeight() < 28.f) {
        return;
    }

    const float desiredHeight = 30.f + (float) jmax(1, (int) parameters.size()) * 34.f;
    area = area.withHeight(jmin(area.getHeight(), desiredHeight));

    auto header = area.reduced(8.f, 5.f).removeFromTop(18.f);
    g.setColour(kMutedText);
    g.setFont(FontOptions(10.5f, Font::bold));
    g.drawText("vertex params", header, Justification::centred);

    for (int i = 0; i < (int) parameters.size(); ++i) {
        const auto& parameter = parameters[(size_t) i];
        const auto row = vertexParameterRowBounds(area, i);
        const auto labelBox = vertexLabelBounds(row);
        const auto guideBox = vertexGuideBounds(row);
        const auto rail = vertexParameterRailBounds(row);
        const String guideLabel = i < (int) guideAttachmentLabels.size()
                ? guideAttachmentLabels[(size_t) i]
                : String();

        drawSliderRowBody(g, row);

        g.setColour(kMutedText);
        g.setFont(FontOptions(10.5f, Font::bold));
        g.drawText(parameter.label.toLowerCase(), labelBox, Justification::centredLeft);

        const float range = parameter.maximum - parameter.minimum;
        const float normalized = range > 0.f
                ? jlimit(0.f, 1.f, (parameter.value - parameter.minimum) / range)
                : 0.f;

        g.setColour(Colour(0xff15191e).withAlpha(0.88f));
        g.fillRect(rail);
        g.setColour(Colour(0xffb7bec7).withAlpha(0.84f));
        g.fillRect(rail.withWidth(rail.getWidth() * normalized));

        g.setColour(guideLabel.isEmpty()
                ? Colour(0xff15191e)
                : Colour(0xff202833));
        g.fillRect(guideBox);
        g.setColour(guideLabel.isEmpty()
                ? Colour(0xff59606a).withAlpha(0.45f)
                : Colour(0xff70a7ff).withAlpha(0.72f));
        g.drawRect(guideBox, 1.f);

        if (guideLabel.isNotEmpty()) {
            g.setColour(Colour(0xff70a7ff).withAlpha(0.90f));
            g.setFont(FontOptions(10.5f, Font::bold));
            g.drawText(guideLabel, guideBox.reduced(3.f, 0.f), Justification::centred);
        } else {
            g.setColour(kMutedText.withAlpha(0.82f));
            Path guideCurve;
            guideCurve.startNewSubPath(guideBox.getX() + 6.f, guideBox.getCentreY() + 4.f);
            guideCurve.cubicTo(
                    guideBox.getX() + 13.f,
                    guideBox.getY() + 3.f,
                    guideBox.getRight() - 17.f,
                    guideBox.getBottom() - 3.f,
                    guideBox.getRight() - 10.f,
                    guideBox.getCentreY() - 3.f);
            g.strokePath(guideCurve, PathStrokeType(1.2f));
            g.fillEllipse(Rectangle<float>(3.f, 3.f)
                    .withCentre({ guideBox.getX() + 7.f, guideBox.getCentreY() + 4.f }));
            g.fillEllipse(Rectangle<float>(3.f, 3.f)
                    .withCentre({ guideBox.getRight() - 11.f, guideBox.getCentreY() - 3.f }));
        }

        Path chevron;
        chevron.startNewSubPath(guideBox.getRight() - 8.f, guideBox.getCentreY() - 2.f);
        chevron.lineTo(guideBox.getRight() - 5.f, guideBox.getCentreY() + 2.f);
        chevron.lineTo(guideBox.getRight() - 2.f, guideBox.getCentreY() - 2.f);
        g.strokePath(chevron, PathStrokeType(1.1f));
    }
}

Rectangle<float> TrimeshSidePanelRenderer::morphCubeBounds(Rectangle<float> sideArea) {
    return cubeStackBounds(sideArea).reduced(0.f, 0.f);
}

Rectangle<float> TrimeshSidePanelRenderer::morphRailBounds(Rectangle<float> sideArea, int axisIndex) {
    auto row = axisRowBounds(sideArea, axisIndex);
    row.removeFromLeft(kAxisLabelW + 10.f);
    row.removeFromRight(kAxisButtonSize * 2.f + kRowButtonGap * 3.f + 6.f);
    return row.withSizeKeepingCentre(jmax(36.f, row.getWidth()), 7.f);
}

Rectangle<float> TrimeshSidePanelRenderer::primaryAxisBounds(Rectangle<float> sideArea, int axisIndex) {
    auto row = axisRowBounds(sideArea, axisIndex);
    return {
            row.getRight() - kAxisButtonSize * 2.f - kRowButtonGap * 2.f,
            row.getCentreY() - kAxisButtonSize * 0.5f,
            kAxisButtonSize,
            kAxisButtonSize
    };
}

Rectangle<float> TrimeshSidePanelRenderer::linkToggleBounds(Rectangle<float> sideArea, int axisIndex) {
    auto row = axisRowBounds(sideArea, axisIndex);
    return {
            row.getRight() - kAxisButtonSize - kRowButtonGap,
            row.getCentreY() - kAxisButtonSize * 0.5f,
            kAxisButtonSize,
            kAxisButtonSize
    };
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterPanelBounds(Rectangle<float> sideArea) {
    return vertexPanelColumnBounds(sideArea);
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterRowBounds(
        Rectangle<float> parameterArea,
        int parameterIndex) {
    auto rows = parameterArea.reduced(8.f, 0.f);
    const float headerHeight = 30.f;
    const float gap = 5.f;
    const float rowHeight = jmin(
            29.f,
            jmax(0.f, (rows.getHeight() - headerHeight - gap * (float) (kVertexParamCount - 1)) / kVertexParamCount));

    rows.removeFromTop(headerHeight);
    rows.translate(0.f, (float) parameterIndex * (rowHeight + gap));
    return rows.removeFromTop(rowHeight);
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterRailBounds(Rectangle<float> parameterRow) {
    parameterRow.removeFromLeft(kVertexLabelW + kVertexCellGap);
    parameterRow.removeFromRight(kVertexGuideW + kVertexCellGap * 2.f);
    return parameterRow.withTrimmedRight(8.f)
            .withSizeKeepingCentre(jmax(4.f, parameterRow.getWidth() - 8.f), 8.f);
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterGuideBounds(Rectangle<float> parameterRow) {
    return vertexGuideBounds(parameterRow);
}

}

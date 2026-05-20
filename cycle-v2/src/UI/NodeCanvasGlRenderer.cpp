#include "NodeCanvasGlRenderer.h"

#include <cmath>

using namespace juce;
namespace gl = juce::gl;

namespace CycleV2 {

namespace {

const Colour kBackground  { 0xff101318 };
const Colour kGridMajor   { 0x065b6370 };
const Colour kGridMinor   { 0x022f363f };
const Colour kTopGlow     { 0x18162531 };
const Colour kBottomGlow  { 0x00162531 };

float wrappedGridStart(float pan, float step) {
    float start = std::fmod(pan, step);

    if (start > 0.f) {
        start -= step;
    }

    return start;
}

}

void NodeCanvasGlRenderer::initialize() {
    gl::glDisable(gl::GL_DEPTH_TEST);
    gl::glEnable(gl::GL_BLEND);
    gl::glBlendFunc(gl::GL_SRC_ALPHA, gl::GL_ONE_MINUS_SRC_ALPHA);
    gl::glDisable(gl::GL_LINE_SMOOTH);
}

void NodeCanvasGlRenderer::shutdown() {
}

void NodeCanvasGlRenderer::renderBackground(
        int width,
        int height,
        float renderingScale,
        float zoom,
        Point<float> pan) {
    if (width <= 0 || height <= 0) {
        return;
    }

    const int viewportWidth = roundToInt((float) width * renderingScale);
    const int viewportHeight = roundToInt((float) height * renderingScale);

    gl::glDisable(gl::GL_DEPTH_TEST);
    gl::glEnable(gl::GL_BLEND);
    gl::glBlendFunc(gl::GL_SRC_ALPHA, gl::GL_ONE_MINUS_SRC_ALPHA);
    gl::glDisable(gl::GL_LINE_SMOOTH);

    gl::glViewport(0, 0, viewportWidth, viewportHeight);
    gl::glMatrixMode(gl::GL_PROJECTION);
    gl::glLoadIdentity();
    gl::glOrtho(0.0, (double) width, (double) height, 0.0, -1.0, 1.0);
    gl::glMatrixMode(gl::GL_MODELVIEW);
    gl::glLoadIdentity();

    const Rectangle<float> bounds(0.f, 0.f, (float) width, (float) height);
    const float minorStep = 32.f * zoom;
    const float majorStep = minorStep * 4.f;

    setColour(kBackground);
    fillRect(bounds);
    drawGridLines(bounds, pan, minorStep, kGridMinor);
    drawGridLines(bounds, pan, majorStep, kGridMajor);

    gl::glBegin(gl::GL_QUADS);
    setColour(kTopGlow);
    gl::glVertex2f(0.f, 0.f);
    gl::glVertex2f((float) width, 0.f);
    setColour(kBottomGlow);
    gl::glVertex2f((float) width, (float) height);
    gl::glVertex2f(0.f, (float) height);
    gl::glEnd();
}

void NodeCanvasGlRenderer::renderCable(
        const Path& path,
        Point<float> source,
        Point<float> dest,
        Colour colour,
        float cableScale,
        bool selected,
        bool attachment,
        bool invalid) {
    const auto segments = flattenPath(path);

    if (segments.empty()) {
        return;
    }

    if (attachment) {
        drawDashedSegments(
                segments,
                colour.withAlpha(selected ? 0.24f : 0.20f),
                (selected ? 9.f : 7.f) * cableScale,
                8.f * cableScale,
                7.f * cableScale);
        drawDashedSegments(
                segments,
                colour.withAlpha(0.92f),
                (selected ? 3.f : 2.f) * cableScale,
                8.f * cableScale,
                7.f * cableScale);
        if (selected) {
            drawDashedSegments(
                    segments,
                    Colours::white.withAlpha(0.34f),
                    1.1f * cableScale,
                    8.f * cableScale,
                    7.f * cableScale);
        }
    } else {
        drawSegments(
                segments,
                colour.withAlpha(selected ? 0.18f : 0.14f),
                (selected ? 11.f : 9.f) * cableScale);
        drawSegments(
                segments,
                colour.withAlpha(0.92f),
                (selected ? 4.f : 3.f) * cableScale);
        if (selected) {
            drawSegments(
                    segments,
                    Colours::white.withAlpha(0.30f),
                    1.1f * cableScale);
        }

        if (invalid) {
            drawDashedSegments(
                    segments,
                    Colours::white.withAlpha(0.58f),
                    1.4f * cableScale,
                    5.f * cableScale,
                    6.f * cableScale);
        }
    }

    const float endpointRadius = (selected ? 7.f : 5.5f) * cableScale;
    drawCircle(source, endpointRadius, kBackground.withAlpha(0.92f), true);
    drawCircle(source, endpointRadius, colour.withAlpha(0.96f), false);
    drawCircle(dest, endpointRadius - 2.f * cableScale, colour.withAlpha(0.96f), true);
}

void NodeCanvasGlRenderer::renderNodeShell(
        Rectangle<float> bounds,
        float headerHeight,
        float cornerRadius,
        Colour bodyColour,
        Colour headerColour) {
    if (bounds.isEmpty() || headerHeight <= 0.f) {
        return;
    }

    setColour(bodyColour);
    fillRoundedRect(bounds, cornerRadius);

    Rectangle<float> header = bounds.removeFromTop(jmin(headerHeight, bounds.getHeight()));
    setColour(headerColour);
    fillRoundedRect(header, cornerRadius);
    fillRect(header.withTrimmedTop(jmax(0.f, header.getHeight() - cornerRadius)));
}

void NodeCanvasGlRenderer::setColour(Colour colour) {
    gl::glColor4f(
            colour.getFloatRed(),
            colour.getFloatGreen(),
            colour.getFloatBlue(),
            colour.getFloatAlpha());
}

void NodeCanvasGlRenderer::fillRect(Rectangle<float> bounds) {
    gl::glBegin(gl::GL_QUADS);
    gl::glVertex2f(bounds.getX(), bounds.getY());
    gl::glVertex2f(bounds.getRight(), bounds.getY());
    gl::glVertex2f(bounds.getRight(), bounds.getBottom());
    gl::glVertex2f(bounds.getX(), bounds.getBottom());
    gl::glEnd();
}

void NodeCanvasGlRenderer::fillRoundedRect(Rectangle<float> bounds, float radius) {
    if (bounds.isEmpty()) {
        return;
    }

    radius = jlimit(0.f, jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f, radius);

    if (radius <= 0.f) {
        gl::glBegin(gl::GL_QUADS);
        gl::glVertex2f(bounds.getX(), bounds.getY());
        gl::glVertex2f(bounds.getRight(), bounds.getY());
        gl::glVertex2f(bounds.getRight(), bounds.getBottom());
        gl::glVertex2f(bounds.getX(), bounds.getBottom());
        gl::glEnd();
        return;
    }

    gl::glBegin(gl::GL_QUADS);
    gl::glVertex2f(bounds.getX() + radius, bounds.getY());
    gl::glVertex2f(bounds.getRight() - radius, bounds.getY());
    gl::glVertex2f(bounds.getRight() - radius, bounds.getBottom());
    gl::glVertex2f(bounds.getX() + radius, bounds.getBottom());

    gl::glVertex2f(bounds.getX(), bounds.getY() + radius);
    gl::glVertex2f(bounds.getRight(), bounds.getY() + radius);
    gl::glVertex2f(bounds.getRight(), bounds.getBottom() - radius);
    gl::glVertex2f(bounds.getX(), bounds.getBottom() - radius);
    gl::glEnd();

    fillCornerFan({ bounds.getX() + radius, bounds.getY() + radius },
                  radius,
                  MathConstants<float>::pi,
                  MathConstants<float>::pi * 1.5f);
    fillCornerFan({ bounds.getRight() - radius, bounds.getY() + radius },
                  radius,
                  MathConstants<float>::pi * 1.5f,
                  MathConstants<float>::twoPi);
    fillCornerFan({ bounds.getRight() - radius, bounds.getBottom() - radius },
                  radius,
                  0.f,
                  MathConstants<float>::halfPi);
    fillCornerFan({ bounds.getX() + radius, bounds.getBottom() - radius },
                  radius,
                  MathConstants<float>::halfPi,
                  MathConstants<float>::pi);
}

void NodeCanvasGlRenderer::fillCornerFan(Point<float> centre, float radius, float startAngle, float endAngle) {
    constexpr int segments = 8;

    gl::glBegin(gl::GL_TRIANGLE_FAN);
    gl::glVertex2f(centre.x, centre.y);

    for (int i = 0; i <= segments; ++i) {
        const float phase = startAngle + (endAngle - startAngle) * (float) i / (float) segments;
        gl::glVertex2f(
                centre.x + (float) dsp::FastMathApproximations::cos((double) phase) * radius,
                centre.y + (float) dsp::FastMathApproximations::sin((double) phase) * radius);
    }

    gl::glEnd();
}

void NodeCanvasGlRenderer::drawLine(Point<float> start, Point<float> end) {
    gl::glBegin(gl::GL_LINES);
    gl::glVertex2f(start.x, start.y);
    gl::glVertex2f(end.x, end.y);
    gl::glEnd();
}

void NodeCanvasGlRenderer::drawCircle(Point<float> centre, float radius, Colour colour, bool filled) {
    if (radius <= 0.f) {
        return;
    }

    setColour(colour);

    if (filled) {
        gl::glBegin(gl::GL_TRIANGLE_FAN);
        gl::glVertex2f(centre.x, centre.y);
    } else {
        gl::glLineWidth(jmax(1.f, radius * 0.24f));
        gl::glBegin(gl::GL_LINE_LOOP);
    }

    constexpr int segments = 28;
    for (int i = 0; i <= segments; ++i) {
        const float phase = MathConstants<float>::twoPi * (float) i / (float) segments;
        gl::glVertex2f(
                centre.x + (float) dsp::FastMathApproximations::cos((double) phase) * radius,
                centre.y + (float) dsp::FastMathApproximations::sin((double) phase) * radius);
    }

    gl::glEnd();
}

void NodeCanvasGlRenderer::drawContinuousStroke(const std::vector<LineSegment>& segments, float width) {
    if (segments.empty() || width <= 0.f) {
        return;
    }

    std::vector<Point<float>> points;
    points.reserve(segments.size() + 1);
    points.push_back(segments.front().start);

    for (const auto& segment : segments) {
        if (segment.end != points.back()) {
            points.push_back(segment.start);
        }

        points.push_back(segment.end);
    }

    if (points.size() < 2) {
        return;
    }

    gl::glLineWidth(jmax(1.f, width));
    gl::glBegin(gl::GL_LINE_STRIP);

    for (const auto& point : points) {
        gl::glVertex2f(point.x, point.y);
    }

    gl::glEnd();
    gl::glLineWidth(1.f);
}

std::vector<NodeCanvasGlRenderer::LineSegment> NodeCanvasGlRenderer::flattenPath(const Path& path) {
    std::vector<LineSegment> segments;
    PathFlatteningIterator iter(path);

    while (iter.next()) {
        segments.push_back({
                { iter.x1, iter.y1 },
                { iter.x2, iter.y2 }
        });
    }

    return segments;
}

void NodeCanvasGlRenderer::drawSegments(
        const std::vector<LineSegment>& segments,
        Colour colour,
        float width) {
    if (segments.empty()) {
        return;
    }

    setColour(colour);
    drawContinuousStroke(segments, width);
}

void NodeCanvasGlRenderer::drawDashedSegments(
        const std::vector<LineSegment>& segments,
        Colour colour,
        float width,
        float dashLength,
        float gapLength) {
    if (segments.empty() || dashLength <= 0.f) {
        return;
    }

    const float period = dashLength + jmax(0.f, gapLength);
    std::vector<LineSegment> currentDash;
    float distance = 0.f;

    auto flushDash = [&]() {
        if (!currentDash.empty()) {
            setColour(colour);
            drawContinuousStroke(currentDash, width);
            currentDash.clear();
        }
    };

    for (const auto& segment : segments) {
        const Point<float> vector = segment.end - segment.start;
        const float length = vector.getDistanceFromOrigin();

        if (length <= 0.f) {
            continue;
        }

        float local = 0.f;

        while (local < length) {
            const float pattern = std::fmod(distance + local, period);
            const bool drawingDash = pattern < dashLength;
            const float remainingInPattern = drawingDash ? dashLength - pattern : period - pattern;
            const float run = jmin(length - local, remainingInPattern);

            if (drawingDash && run > 0.f) {
                const float startT = local / length;
                const float endT = (local + run) / length;
                const Point<float> start = segment.start + vector * startT;
                const Point<float> end = segment.start + vector * endT;
                currentDash.push_back({ start, end });
            } else {
                flushDash();
            }

            local += jmax(0.5f, run);
        }

        distance += length;
    }

    flushDash();
}

void NodeCanvasGlRenderer::drawGridLines(
        Rectangle<float> bounds,
        Point<float> pan,
        float step,
        Colour colour) {
    if (step <= 0.f) {
        return;
    }

    setColour(colour);

    for (float x = wrappedGridStart(pan.x, step); x < bounds.getRight(); x += step) {
        drawLine({ x, bounds.getY() }, { x, bounds.getBottom() });
    }

    for (float y = wrappedGridStart(pan.y, step); y < bounds.getBottom(); y += step) {
        drawLine({ bounds.getX(), y }, { bounds.getRight(), y });
    }
}

}

#include "NodeCanvasGlRenderer.h"

#include <cmath>

using namespace juce;
namespace gl = juce::gl;

namespace CycleV2 {

namespace {

const Colour kBackground  { 0xff101318 };
const Colour kGridMajor   { 0x245b6370 };
const Colour kGridMinor   { 0x102f363f };
const Colour kTopGlow     { 0x28162531 };
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

    gl::glViewport(0, 0, viewportWidth, viewportHeight);
    gl::glMatrixMode(gl::GL_PROJECTION);
    gl::glLoadIdentity();
    gl::glOrtho(0.0, (double) width, (double) height, 0.0, -1.0, 1.0);
    gl::glMatrixMode(gl::GL_MODELVIEW);
    gl::glLoadIdentity();

    const Rectangle<float> bounds(0.f, 0.f, (float) width, (float) height);
    const float minorStep = 32.f * zoom;
    const float majorStep = minorStep * 4.f;

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

void NodeCanvasGlRenderer::setColour(Colour colour) {
    gl::glColor4f(
            colour.getFloatRed(),
            colour.getFloatGreen(),
            colour.getFloatBlue(),
            colour.getFloatAlpha());
}

void NodeCanvasGlRenderer::fillRect(Rectangle<float> bounds) {
    setColour(kBackground);
    gl::glBegin(gl::GL_QUADS);
    gl::glVertex2f(bounds.getX(), bounds.getY());
    gl::glVertex2f(bounds.getRight(), bounds.getY());
    gl::glVertex2f(bounds.getRight(), bounds.getBottom());
    gl::glVertex2f(bounds.getX(), bounds.getBottom());
    gl::glEnd();
}

void NodeCanvasGlRenderer::drawLine(Point<float> start, Point<float> end) {
    gl::glBegin(gl::GL_LINES);
    gl::glVertex2f(start.x, start.y);
    gl::glVertex2f(end.x, end.y);
    gl::glEnd();
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

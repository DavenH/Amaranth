#include "ScopedGLScissor.h"

using namespace juce;
using namespace gl;

ScopedGLScissor::ScopedGLScissor(Rectangle<float> bounds, float scaleFactor) {
    glGetBooleanv(GL_SCISSOR_TEST, &wasEnabled);
    glGetIntegerv(GL_SCISSOR_BOX, previousBox);

    GLint viewportValues[4] {};
    glGetIntegerv(GL_VIEWPORT, viewportValues);
    const Rectangle<int> box = boxFor(
            bounds,
            scaleFactor,
            { viewportValues[0], viewportValues[1], viewportValues[2], viewportValues[3] });
    glEnable(GL_SCISSOR_TEST);
    glScissor(box.getX(), box.getY(), box.getWidth(), box.getHeight());
}

ScopedGLScissor::~ScopedGLScissor() {
    if (wasEnabled != 0) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }

    glScissor(previousBox[0], previousBox[1], previousBox[2], previousBox[3]);
}

Rectangle<int> ScopedGLScissor::boxFor(
        Rectangle<float> bounds,
        float scaleFactor,
        Rectangle<int> viewport) {
    const int x = jmax(0, roundToInt(bounds.getX() * scaleFactor));
    const int y = jmax(
            0,
            viewport.getBottom() - roundToInt(bounds.getBottom() * scaleFactor));
    const int width = jmax(1, roundToInt(bounds.getWidth() * scaleFactor));
    const int height = jmax(1, roundToInt(bounds.getHeight() * scaleFactor));
    return { x, y, width, height };
}

#include "CurvePanelDrawing.h"

#include "CommonGfx.h"

namespace CurvePanelDrawing {

void drawWaveshaperBounds(Canvas& canvas, float padding) {
    const int innerRight = canvas.scaleX(1.f - padding);
    const int innerLeft = canvas.scaleX(padding);
    const int low = canvas.scaleY(1.f - padding);
    const int high = canvas.scaleY(padding);

    canvas.gfx.setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
    canvas.gfx.fillRect(innerLeft, canvas.height, innerRight, high, false);
    canvas.gfx.fillRect(innerLeft, 0, innerRight, low, false);
    canvas.gfx.fillRect(0, canvas.height, innerLeft, 0, false);
    canvas.gfx.fillRect(canvas.width, canvas.height, innerRight, 0, false);

    canvas.gfx.disableSmoothing();
    canvas.gfx.setCurrentLineWidth(1.f);
    canvas.gfx.setCurrentColour(0.3f, 0.3f, 0.3f, 0.5f);
    canvas.gfx.drawRect(innerLeft, high, innerRight, low, false);
    canvas.gfx.enableSmoothing();
}

void drawGuideBackground(Canvas& canvas, const GuideView& view) {
    const int innerLeft = canvas.scaleX(view.padding);
    const int innerRight = canvas.scaleX(1.f - view.padding);

    canvas.gfx.setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
    canvas.gfx.fillRect(0, canvas.height, innerLeft, 0, false);
    canvas.gfx.fillRect(canvas.width, canvas.height, innerRight, 0, false);

    canvas.gfx.disableSmoothing();
    canvas.gfx.setCurrentColour(0.2f, 0.2f, 0.2f, 0.5f);
    canvas.gfx.drawLine(innerLeft, canvas.height, innerLeft, 0, false);
    canvas.gfx.drawLine(innerRight, canvas.height, innerRight, 0, false);
    canvas.gfx.enableSmoothing();

    const float right = 1.f - view.padding;
    const float size = 1.f - 2.f * view.padding;
    canvas.gfx.setCurrentColour(0.7f, 0.55f, 0.18f, 0.17f);
    canvas.gfx.fillRect(view.padding, 0.5f + 0.5f * view.noise,
            right, 0.5f - 0.5f * view.noise, true);
    canvas.gfx.setCurrentColour(0.7f, 0.08f, 0.5f, 0.17f);
    canvas.gfx.fillRect(view.padding, 0.5f + 0.5f * view.dcOffset,
            right, 0.5f - 0.5f * view.dcOffset, true);
    canvas.gfx.setCurrentColour(0.3f, 0.6f, 0.9f, 0.17f);
    canvas.gfx.fillRect(0.5f - 0.5f * size * view.phase, 0.f,
            0.5f + 0.5f * size * view.phase, 1.f, true);
}

void drawImpulseResponseBackground(Canvas& canvas, float padding) {
    const int innerLeft = canvas.scaleX(padding);
    canvas.gfx.setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
    canvas.gfx.fillRect(0, canvas.height, innerLeft, 0, false);
    canvas.gfx.disableSmoothing();
    canvas.gfx.setCurrentColour(0.2f, 0.2f, 0.2f);
    canvas.gfx.drawLine(innerLeft, canvas.height, innerLeft, 0, false);
    canvas.gfx.enableSmoothing();
}

void drawImpulseResponseBounds(Canvas& canvas, float padding) {
    const int innerLeft = canvas.scaleX(padding);
    canvas.gfx.setCurrentColour(0.3f, 0.3f, 0.3f, 0.5f);
    canvas.gfx.drawRect(0, canvas.height, innerLeft, 0, false);
}

}

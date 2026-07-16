#pragma once

#include <functional>

class CommonGfx;

namespace CurvePanelDrawing {

struct Canvas {
    CommonGfx& gfx;
    int width {};
    int height {};
    std::function<int(float)> scaleX;
    std::function<int(float)> scaleY;
};

struct GuideView {
    float padding {};
    float noise {};
    float dcOffset {};
    float phase {};
};

void drawWaveshaperBounds(Canvas& canvas, float padding);
void drawGuideBackground(Canvas& canvas, const GuideView& view);
void drawImpulseResponseBackground(Canvas& canvas, float padding);
void drawImpulseResponseBounds(Canvas& canvas, float padding);

}

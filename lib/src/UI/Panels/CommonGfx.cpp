#include "CommonGfx.h"
#include "Panel.h"
#include "../../Array/ScopedAlloc.h"
#include "../../Inter/Interactor.h"
#include "../../Inter/PanelState.h"
#include "../../Util/Arithmetic.h"
#include "../../Definitions.h"

void CommonGfx::scaleIfNecessary(bool scale, BufferXY& xy) {
    if (scale) {
        panel->applyScaleX(xy.x);
        panel->applyScaleY(xy.y);
    }
}

void CommonGfx::scaleIfNecessary(bool scale, float& x1, float& y1) {
    if (scale) {
        x1 = panel->sx(x1);
        y1 = panel->sy(y1);
    }
}

void CommonGfx::scaleIfNecessary(bool scale, float& x1, float& y1, float& x2, float& y2) {
    if (scale) {
        x1 = panel->sx(x1);
        x2 = panel->sx(x2);
        y1 = panel->sy(y1);
        y2 = panel->sy(y2);
    }
}

void CommonGfx::drawBackground(const Rectangle<int>& bounds, bool fillBackground) {
    if (bounds.getWidth() == 0 || bounds.getHeight() == 0) {
        return;
    }

    if (fillBackground) {
        Interactor* itr = panel->interactor;
        Range<float> xLimits = itr->vertexLimits[itr->dims.x];
        Range<float> yLimits = itr->vertexLimits[itr->dims.y];

        setCurrentColour(0.07f, 0.07f, 0.07f);
        fillRect(0, 0, bounds.getWidth(), bounds.getHeight(), false);

        if (xLimits.getLength() > 1.f) {
            setCurrentColour(0.12f, 0.12f, 0.12f);
            fillRect(panel->sx(xLimits.getStart()), 0, panel->sx(0), bounds.getHeight(), false);
            fillRect(panel->sx(1), 0, panel->sx(yLimits.getEnd()), bounds.getHeight(), false);
        }

        if (yLimits.getLength() > 1.f) {
            setCurrentColour(0.12f, 0.12f, 0.12f);
            fillRect(0, panel->sy(yLimits.getStart()), bounds.getWidth(), panel->sy(0), false);
            fillRect(0, panel->sy(1), bounds.getWidth(), panel->sy(yLimits.getEnd()), false);
        }
    }

    {
        float minorLevel = panel->minorBrightness;
        setCurrentColour(minorLevel, minorLevel, minorLevel);
        disableSmoothing();
        setCurrentLineWidth(1.f);

        float topY = panel->sy(panel->bgPaddingTop);
        float botY = panel->sy(1 - panel->bgPaddingBttm);

        Buffer<float> xMinor, xMajor, yMinor, yMajor, xMinScaled;
        int leftMinorIdx = 0, rightMinorIdx = 0;

        xMinor = panel->vertMinorLines;
        yMinor = panel->horzMinorLines;
        xMajor = panel->vertMajorLines;
        yMajor = panel->horzMajorLines;

        float left = jmax(xMinor.front(), panel->invertScaleX(0));
        float right = jmin(xMinor.back(), panel->invertScaleX(panel->comp->getWidth()));

        leftMinorIdx = jmax(0, Arithmetic::binarySearch(left, xMinor));
        rightMinorIdx = jmin(xMinor.size() - 1, Arithmetic::binarySearch(right, xMinor));

        panel->xBuffer.ensureSize(rightMinorIdx - leftMinorIdx);

        xMinScaled = panel->xBuffer.withSize(rightMinorIdx - leftMinorIdx);

        float leftNZ = panel->sx(panel->bgPaddingLeft);
        float rightNZ = panel->sx(1 - panel->bgPaddingRight);

        if (!xMinScaled.empty()) {
            xMinor.offset(leftMinorIdx).copyTo(xMinScaled);
            panel->applyScaleX(xMinScaled);

            for (float i : xMinScaled) {
                drawLine(i, topY, i, botY, false);
            }
        }

        for (float i : yMinor) {
            float y = panel->sy(i);
            drawLine(leftNZ, y, rightNZ, y, false);
        }

        float majorLevel = panel->majorBrightness;
        setCurrentColour(majorLevel, majorLevel, majorLevel);

        if (!xMajor.empty()) {
            panel->xBuffer.ensureSize(xMajor.size());
            Buffer xMajScaled(panel->xBuffer.withSize(xMajor.size()));
            xMajor.copyTo(xMajScaled);
            panel->applyScaleX(xMajScaled);

            for (float i : xMajScaled) {
                drawLine(i, topY, i, botY, false);
            }
        }

        for (float i : yMajor) {
            float y = panel->sy(i);
            drawLine(leftNZ, y, rightNZ, y, false);
        }
    }
}

void CommonGfx::drawFinalSelection() {
    if (panel->interactor->getSelected().empty()) {
        return;
    }

    PanelState& state = panel->interactor->state;

    vector<Vertex2>& corners = panel->interactor->selectionCorners;

    if (corners.empty()) {
        return;
    }

    setCurrentColour(0.4f, 0.4f, 0.4f, 0.25f);
    fillRect(corners[0].x, corners[0].y, corners[2].x, corners[2].y, false);

    disableSmoothing();
    setCurrentColour(0.6f, 0.6f, 0.6f, 1.f);
    setCurrentLineWidth(2.f);

    drawRect(corners[0].x, corners[0].y, corners[2].x, corners[2].y, false);

    int corner = getStateValue(HighlitCorner);

    if (corner >= PanelState::Left && corner < PanelState::MoveHandle) {
        int next = (corner + 1) % (int) corners.size();

        Vertex2& v1 = corners[corner];
        Vertex2& v2 = corners[next];

        Vertex2 d[4];

        if (corner % 4 == 0) {
            d[0] = Vertex2(1, 1);
            d[1] = Vertex2(1, -1);
            d[2] = Vertex2(-1, 1);
            d[3] = Vertex2(-1, -1);
        } else if (corner % 4 == 2) {
            d[0] = Vertex2(1, 1);
            d[1] = Vertex2(-1, 1);
            d[2] = Vertex2(1, -1);
            d[3] = Vertex2(-1, -1);

        #if USE_CORNERS
            }
            else if (corner % 4 == 1)
            {
                d[0] = Vertex2(0, 1.6f);
                d[1] = Vertex2(-1.6f, 0);
                d[2] = Vertex2(1.6f, 0);
                d[3] = Vertex2(0, -1.6f);
            }
            else if (corner % 4 == 3)
            {
                d[0] = Vertex2(0, -1.6f);
                d[1] = Vertex2(-1.6f, 0);
                d[2] = Vertex2(1.6f, 0);
                d[3] = Vertex2(0, 1.6f);
        #endif
        }

        setCurrentLineWidth(1.f);
        setCurrentColour(0.9f, 0.9f, 0.9f, 1.f);
        drawLine(v1, v2, false);
    }

    enableSmoothing();
    drawTexture(panel->grabTex);
}

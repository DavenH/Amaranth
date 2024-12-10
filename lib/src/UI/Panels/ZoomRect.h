#ifndef _zoomrect_h
#define _zoomrect_h

class ZoomRect {
public:
    float x, y, w, h;
    float xMaximum, xMinimum, yMinimum, yMaximum;

    ZoomRect()
            : x(0), y(0), w(1), h(1),
              xMinimum(0.f), xMaximum(1.f),
              yMinimum(0.f), yMaximum(1.f) {
    }
    ZoomRect(float _x, float _y, float _w, float _h)
            : x(_x), y(_y), w(_w), h(_h),
              xMinimum(0.f), xMaximum(1.f),
              yMinimum(0.f), yMaximum(1.f) {
    }

    bool operator==(const ZoomRect& other) {
        return x == other.x && y == other.y && w == other.w && h == other.h;
    }

    bool operator!=(const ZoomRect& other) {
        return !this->operator==(other);
    }

    bool isIdentity() {
        return x == 0 && y == 0 && w == 1 && h == 1;
    }
};

#endif

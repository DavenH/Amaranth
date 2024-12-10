#pragma once

#include "../Array/CircleBuffer.h"

class HermiteState {
public:
    void init(Buffer<float> memory) {
        ring = memory;
        reset(1.);
    }

    void reset(double ratio) {
        srcToDstRatio = ratio;
        phase = 0;
        yn2 = 0;
        yn1 = 0;
        y0 = 0;
        y1 = 0;
        y2 = 0;
        y3 = 0;
    }

    void write(float input) {
        yn2 = yn1;
        yn1 = y0;
        y0 = y1;
        y1 = y2;
        y2 = y3;
        y3 = input;
    }

    float at(float x) {
        float c1 = 1 / 12.f * (yn2 - y2) + 2 / 3.f * (y1 - yn1);
        float c2 = 5 / 4.f * yn1 - 7 / 3.f * y0 + 5 / 3.f * y1 - 1 / 2.f * y2 + 1 / 12.f * y3 - 1 / 6.f * yn2;
        float c3 = 1 / 12.f * (yn2 - y3) + 7 / 12.f * (y2 - yn1) + 4 / 3.f * (y0 - y1);

        return ((c3 * x + c2) * x + c1) * x + y0;
    }

    int resample(Buffer<float> src, Buffer<float> dst) {
        if (src.empty())
            return 0;

        for (int i = 0; i < src.size(); ++i) {
            write(src[i]);

            while (phase < double(i + 1)) {
                float x = phase - (long) phase;
                ring.write(at(x));
                phase += srcToDstRatio;
            }
        }

        phase -= (double) src.size();

        return ring.readArray(dst);
    }

    float yn2, yn1, y0, y1, y2, y3;
    double srcToDstRatio;
    double phase;

private:
    CircleBuffer ring;
};
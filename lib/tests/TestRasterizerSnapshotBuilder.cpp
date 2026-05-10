#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Curve.h"
#include "../src/Curve/Rasterization/Builders/RasterizerSnapshotBuilder.h"
#include "../src/Obj/ColorPoint.h"

namespace {
    std::vector<float> copyBuffer(Buffer<float> buffer) {
        std::vector<float> values;
        values.reserve(buffer.size());

        for (int i = 0; i < buffer.size(); ++i) {
            values.emplace_back(buffer[i]);
        }

        return values;
    }
}

TEST_CASE("RasterizerSnapshotBuilder publishes RasterizerData contents", "[rasterization][snapshot]") {
    std::vector<Intercept> intercepts {
        Intercept(0.0f, 0.1f),
        Intercept(0.5f, 0.8f),
        Intercept(1.0f, 0.2f),
    };

    std::vector<ColorPoint> colorPoints {
        ColorPoint(nullptr, Vertex2(0.1f, 0.2f), Vertex2(0.3f, 0.4f), Vertex2(0.5f, 0.6f), Vertex::Time),
    };

    std::vector<Curve> curves {
        Curve(intercepts[0], intercepts[1], intercepts[2]),
    };
    curves.front().setResIndex(1);
    curves.front().curveRes = 7;
    curves.front().waveIdx = 3;

    ScopedAlloc<float> memory;
    memory.ensureSize(8);
    Buffer<float> waveX = memory.place(4);
    Buffer<float> waveY = memory.place(4);

    for (int i = 0; i < 4; ++i) {
        waveX[i] = float(i) * 0.25f;
        waveY[i] = 1.f - float(i) * 0.125f;
    }

    Rasterization::RasterizerSnapshotSource source;
    source.intercepts = &intercepts;
    source.colorPoints = &colorPoints;
    source.curves = &curves;
    source.waveform = Rasterization::WaveformBuffers(
            waveX,
            waveY,
            Buffer<float>(),
            Buffer<float>(),
            Buffer<float>(),
            1,
            3);

    RasterizerData data;
    Rasterization::RasterizerSnapshotBuilder().publish(data, source);

    REQUIRE(data.intercepts.size() == intercepts.size());
    REQUIRE(data.colorPoints.size() == colorPoints.size());
    REQUIRE(data.curves.size() == curves.size());
    REQUIRE(data.waveX.size() == waveX.size());
    REQUIRE(data.waveY.size() == waveY.size());
    REQUIRE(data.zeroIndex == source.waveform.zeroIndex);
    REQUIRE(data.oneIndex == source.waveform.oneIndex);

    REQUIRE(copyBuffer(data.waveX) == copyBuffer(waveX));
    REQUIRE(copyBuffer(data.waveY) == copyBuffer(waveY));

    waveX[0] = 99.f;
    waveY[0] = 88.f;

    REQUIRE(data.waveX[0] == 0.f);
    REQUIRE(data.waveY[0] == 1.f);
}

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <future>
#include <memory>

#include "../src/Curve/Curve.h"
#include "../src/Curve/Rasterization/Builders/RasterizerSnapshotBuilder.h"
#include "../src/Curve/Rasterization/Rasterizer/RasterizerViews.h"
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
    source.paddingSize = 4;
    source.wrapsVertices = true;
    source.sampleable = true;

    RasterizerData data;
    Rasterization::RasterizerSnapshotBuilder().publish(data, source);
    Rasterization::SnapshotView snapshot(data);

    REQUIRE(snapshot.intercepts().size() == intercepts.size());
    REQUIRE(snapshot.colorPoints().size() == colorPoints.size());
    REQUIRE(snapshot.curves().size() == curves.size());
    REQUIRE(snapshot.waveX().size() == waveX.size());
    REQUIRE(snapshot.waveY().size() == waveY.size());
    REQUIRE(snapshot.zeroIndex() == source.waveform.zeroIndex);
    REQUIRE(snapshot.oneIndex() == source.waveform.oneIndex);
    REQUIRE(snapshot.paddingSize() == source.paddingSize);
    REQUIRE(snapshot.wrapsVertices() == source.wrapsVertices);
    REQUIRE(snapshot.isSampleable() == source.sampleable);

    REQUIRE(copyBuffer(snapshot.waveX()) == copyBuffer(waveX));
    REQUIRE(copyBuffer(snapshot.waveY()) == copyBuffer(waveY));

    waveX[0] = 99.f;
    waveY[0] = 88.f;

    REQUIRE(snapshot.waveX()[0] == 0.f);
    REQUIRE(snapshot.waveY()[0] == 1.f);
}

TEST_CASE("Retained rasterizer snapshots survive publication and producer destruction", "[rasterization][snapshot]") {
    Rasterization::SnapshotView retained;
    {
        RasterizerData data;
        ScopedAlloc<float> memory(4);
        Buffer<float> waveX = memory.place(2);
        Buffer<float> waveY = memory.place(2);
        waveX[0] = 0.f;
        waveX[1] = 1.f;
        waveY[0] = 0.25f;
        waveY[1] = 0.75f;

        Rasterization::RasterizerSnapshotSource first;
        first.waveform = { waveX, waveY, {}, {}, {}, 0, 1 };
        first.paddingSize = 2;
        first.sampleable = true;
        Rasterization::RasterizerSnapshotBuilder().publish(data, first);
        retained = Rasterization::SnapshotView(data);

        waveY[0] = 0.5f;
        Rasterization::RasterizerSnapshotSource second = first;
        second.paddingSize = 4;
        Rasterization::RasterizerSnapshotBuilder().publish(data, second);
        Rasterization::SnapshotView current(data);

        REQUIRE(retained.paddingSize() == 2);
        REQUIRE(retained.waveY()[0] == 0.25f);
        REQUIRE(current.paddingSize() == 4);
        REQUIRE(current.waveY()[0] == 0.5f);
    }

    REQUIRE(retained.paddingSize() == 2);
    REQUIRE(retained.waveY()[0] == 0.25f);
}

TEST_CASE("A retained rasterizer reader does not block publication", "[rasterization][snapshot]") {
    RasterizerData data;
    Rasterization::RasterizerSnapshotSource source;
    source.paddingSize = 1;
    Rasterization::RasterizerSnapshotBuilder builder;
    builder.publish(data, source);
    Rasterization::SnapshotView retained(data);

    source.paddingSize = 2;
    auto publication = std::async(std::launch::async, [&] {
        builder.publish(data, source);
    });
    const auto status = publication.wait_for(std::chrono::milliseconds(100));

    retained = Rasterization::SnapshotView();
    publication.get();
    REQUIRE(status == std::future_status::ready);
    REQUIRE(Rasterization::SnapshotView(data).paddingSize() == 2);
}

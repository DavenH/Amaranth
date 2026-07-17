#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <Array/ScopedAlloc.h>
#include <Curve/Curve.h>
#include <Curve/Mesh/Intercept.h>
#include <Obj/ColorPoint.h>

struct RasterizerSnapshotData {
    int zeroIndex {};
    int oneIndex {};
    int paddingSize {};
    bool wrapsVertices {};
    bool sampleable {};
    ScopedAlloc<float> buffer;
    Buffer<float> waveX;
    Buffer<float> waveY;
    std::vector<ColorPoint> colorPoints;
    std::vector<Intercept> intercepts;
    std::vector<Curve> curves;
};

class RasterizerData {
public:
    RasterizerData() : published(std::make_shared<const RasterizerSnapshotData>()) {}

    std::shared_ptr<const RasterizerSnapshotData> snapshot() const {
        return std::atomic_load_explicit(&published, std::memory_order_acquire);
    }

    void publish(std::shared_ptr<const RasterizerSnapshotData> next) {
        jassert(next != nullptr);
        std::atomic_store_explicit(&published, std::move(next), std::memory_order_release);
    }

    int paddingSize {};
    bool wrapsVertices {};

private:
    std::shared_ptr<const RasterizerSnapshotData> published;
};

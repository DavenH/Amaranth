#pragma once

#include <vector>

#include "../../../Array/ScopedAlloc.h"
#include "../../Curve.h"
#include "../../Intercept.h"
#include "../../../Obj/ColorPoint.h"

struct V2RasterArtifacts {
    const std::vector<Intercept>* intercepts{nullptr};

    const std::vector<Curve>* curves{nullptr};

    const std::vector<ColorPoint>* colorPoints{nullptr};

    Buffer<float> waveX;
    Buffer<float> waveY;
    Buffer<float> diffX;
    Buffer<float> slope;
    int zeroIndex{0};
    int oneIndex{0};

    void clear() noexcept {
        *this = V2RasterArtifacts{};
    }
};

class V2RasterizerPipeline {
public:
    virtual ~V2RasterizerPipeline() = default;

    bool renderArtifacts(V2RasterArtifacts& artifacts) noexcept {
        artifacts.clear();
        if (! renderIntercepts(artifacts)) {
            return false;
        }

        return renderWaveform(artifacts);
    }

    virtual bool renderIntercepts(V2RasterArtifacts& artifacts) noexcept = 0;
    virtual bool renderWaveform(V2RasterArtifacts& artifacts) noexcept = 0;
};

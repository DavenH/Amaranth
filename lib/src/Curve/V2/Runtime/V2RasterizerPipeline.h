#pragma once

#include <vector>

#include <Array/ScopedAlloc.h>
#include <Curve/Curve.h>
#include <Curve/Intercept.h>
#include <Obj/ColorPoint.h>
#include <Curve/V2/Runtime/V2RenderTypes.h>

struct V2RasterArtifacts {
    const std::vector<Intercept>* intercepts{nullptr};

    const std::vector<Curve>* curves{nullptr};

    const std::vector<ColorPoint>* colorPoints{nullptr};
    const std::vector<V2DeformRegion>* deformRegions{nullptr};

    V2WaveBuffers waveBuffers;
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

    bool renderBlock(
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept {
        result = V2RenderResult{};

        if (output.empty() || ! request.isValid()) {
            return false;
        }

        int numSamples = jmin(request.numSamples, output.size());
        if (numSamples <= 0) {
            return false;
        }

        V2RasterArtifacts artifacts;
        if (! renderArtifacts(artifacts)) {
            return false;
        }

        return sampleArtifacts(artifacts, request, output.withSize(numSamples), result);
    }

    virtual bool renderIntercepts(V2RasterArtifacts& artifacts) noexcept = 0;
    virtual bool renderWaveform(V2RasterArtifacts& artifacts) noexcept = 0;

protected:
    virtual bool sampleArtifacts(
        const V2RasterArtifacts& artifacts,
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept = 0;
};

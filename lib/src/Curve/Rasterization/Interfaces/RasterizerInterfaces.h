#pragma once

#include "../../../Array/Buffer.h"
#include "../../../Design/Updating/UpdateType.h"

class Dimensions;
class GuideCurveProvider;
class Mesh;
struct RasterizerData;

namespace Rasterization {
    class GuideCurveBindableRasterizer {
    public:
        virtual ~GuideCurveBindableRasterizer() = default;

        virtual void setGuideCurveProvider(GuideCurveProvider* guideCurveProvider) = 0;
    };

    class MeshBindableRasterizer {
    public:
        virtual ~MeshBindableRasterizer() = default;

        virtual Mesh* getMesh() = 0;
        virtual void setMesh(Mesh* mesh) = 0;
        virtual bool hasEnoughCubesForCrossSection() = 0;
    };

    class RasterizerSampler {
    public:
        virtual ~RasterizerSampler() = default;

        virtual bool isSampleable() = 0;
        virtual bool isSampleableAt(float x) = 0;
        virtual float sampleAt(double angle) = 0;
        virtual float sampleAt(double angle, int& currentIndex) = 0;
        virtual float samplePerfectly(double delta, Buffer<float> buffer, double phase) = 0;
    };

    class RasterizerUpdateTarget {
    public:
        virtual ~RasterizerUpdateTarget() = default;

        virtual void cleanUp() = 0;
        virtual void performUpdate(UpdateType updateType) = 0;
        virtual void reset() = 0;
        virtual void updateRasterizer(UpdateType updateType) = 0;
    };

    class RasterizerVertexDomain {
    public:
        virtual ~RasterizerVertexDomain() = default;

        virtual bool wrapsVertices() const = 0;
        virtual int getPaddingSize() const = 0;
        virtual GuideCurveProvider* getGuideCurveProvider() const = 0;
        virtual void setDims(const Dimensions& dims) = 0;
    };
}

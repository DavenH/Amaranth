#pragma once

#include "../../Array/Buffer.h"
#include "../../Design/Updating/UpdateType.h"

class Dimensions;
class GuideCurveProvider;
class Mesh;
struct RasterizerData;

namespace Rasterization {
    class Rasterizer {
    public:
        virtual ~Rasterizer() = default;

        virtual void setGuideCurveProvider(GuideCurveProvider* guideCurveProvider) = 0;
        virtual Mesh* getMesh() = 0;
        virtual void setMesh(Mesh* mesh) = 0;
        virtual bool hasEnoughCubesForCrossSection() = 0;

        virtual bool isSampleable() = 0;
        virtual bool isSampleableAt(float x) = 0;
        virtual float sampleAt(double angle) = 0;
        virtual float sampleAt(double angle, int& currentIndex) = 0;
        virtual float samplePerfectly(double delta, Buffer<float> buffer, double phase) = 0;

        virtual void cleanUp() = 0;
        virtual void performUpdate(UpdateType updateType) = 0;
        virtual void reset() = 0;
        virtual void updateRasterizer(UpdateType updateType) = 0;

        virtual bool wrapsVertices() const = 0;
        virtual int getPaddingSize() const = 0;
        virtual GuideCurveProvider* getGuideCurveProvider() const = 0;
        virtual RasterizerData& getRasterizerData() = 0;
        virtual const RasterizerData& getRasterizerData() const = 0;
        virtual void setDims(const Dimensions& dims) = 0;
    };
}

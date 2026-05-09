#pragma once

class Dimensions;
class GuideCurveProvider;

namespace Rasterization {
    class RasterizerVertexDomain {
    public:
        virtual ~RasterizerVertexDomain() = default;

        virtual bool wrapsVertices() const = 0;
        virtual int getPaddingSize() const = 0;
        virtual GuideCurveProvider* getGuideCurveProvider() const = 0;
        virtual void setDims(const Dimensions& dims) = 0;
    };
}

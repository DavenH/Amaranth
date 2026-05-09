#pragma once

class GuideCurveProvider;

namespace Rasterization {
    class GuideCurveBindableRasterizer {
    public:
        virtual ~GuideCurveBindableRasterizer() = default;

        virtual void setGuideCurveProvider(GuideCurveProvider* guideCurveProvider) = 0;
    };
}

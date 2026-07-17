#pragma once

#include <Array/ScopedAlloc.h>
#include <Curve/Curve.h>
#include <Curve/Mesh/Intercept.h>
#include <Curve/Rasterization/Policies/Curves/CurvePolicies.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Design/Updating/Updateable.h>

#include "PointListWaveformRasterizer.h"

class Rasterizer2D {
public:
    explicit Rasterizer2D(vector<Intercept>& verts, bool cyclic = false)
            : points(verts), cyclic(cyclic) {}

    void updateCurve(int index, const Intercept& position) {
        vector<Curve>& curves = pointListRasterizer.result().curves;
        jassert(index < curves.size());

        if(index < 1 || index > (int) curves.size() - 2) {
            return;
        }

        Curve& prev = curves[index - 1];
        Curve& curve = curves[index + 0];
        Curve& next = curves[index + 1];

        prev.c.x = position.x;
        prev.c.y = position.y;

        prev.validate();
        prev.recalculateCurve();

        curve.b.x = position.x;
        curve.b.y = position.y;
        curve.b.shp = position.shp;

        curve.updateCurrentIndex();
        curve.validate();
        curve.recalculateCurve();

        next.a.x = position.x;
        next.a.y = position.y;
        next.validate();
        next.recalculateCurve();

        const int pointIndex = index - getPaddingSize();
        if (isPositiveAndBelow(pointIndex, (int) points.size())) {
            points[pointIndex] = position;
        }

        if (pointIndex == 0 || pointIndex == (int) points.size() - 1) {
            renderPointListWaveform();
            return;
        }

        updateWaveform(index);
    }

    void updateWaveform(int index) {
        vector<Curve>& curves = pointListRasterizer.result().curves;
        const int startIndex = jmax(0, index - 2);
        const int endIndex = jmin((int) curves.size() - 1, index + 2);
        const auto request = createRasterizationRequest();

        if (!pointListRasterizer.rebakeAffectedRange(
                    startIndex,
                    endIndex,
                    request)) {
            renderPointListWaveform();
        }
    }

    void performUpdate(UpdateType updateType) {
        if (updateType == Update) {
            updateWaveform();
        }
    }

    void updateWaveform() {
        renderPointListWaveform();
    }

    void cleanUp() {
        pointListRasterizer.cleanUp();
    }

    void validateCurves() {
        const vector<Curve>& curves = pointListRasterizer.result().curves;

        for (int i = 0; i < (int) curves.size() - 1; ++i) {
            jassert(curves[i].b.x == curves[i + 1].a.x);
            jassert(curves[i].c.x == curves[i + 1].b.x);
        }
    }

    Rasterization::RasterizationRequest createRasterizationRequest() const {
        Rasterization::RasterizationRequest request;
        request.cyclic = cyclic;

        return request;
    }

    template<typename T>
    T sampleWithInterval(Buffer<float> buffer, T delta, T phase) {
        return Rasterization::WaveformSampler::sampleWithInterval(
                pointListRasterizer.result().waveform,
                buffer,
                delta,
                phase);
    }

    bool isSampleable() const {
        return pointListRasterizer.sampler().isSampleable();
    }

    Buffer<float> getWaveX() { return pointListRasterizer.result().waveform.waveX; }
    Buffer<float> getWaveY() { return pointListRasterizer.result().waveform.waveY; }
    const Rasterization::RenderResult& result() const { return pointListRasterizer.result(); }

    void setCyclicity(bool isCyclic)    { cyclic = isCyclic;    }
    bool isCyclic() const               { return cyclic;        }
    static int getPaddingSize()         { return 2;             }

    const Rasterization::PointListWaveformRasterizer::Diagnostics& getDiagnostics() const {
        return pointListRasterizer.getDiagnostics();
    }

private:
    void renderPointListWaveform();

protected:
    vector<Intercept>& points;
    Rasterization::PointListWaveformRasterizer pointListRasterizer;
    bool cyclic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rasterizer2D)
};

inline void Rasterizer2D::renderPointListWaveform() {
    if (points.empty()) {
        cleanUp();
        return;
    }

    Rasterization::RasterizationRequest request = createRasterizationRequest();
    request.cyclic = cyclic;

    const auto& result = pointListRasterizer.renderIntercepts(points, request);
    if (!result.sampleable) {
        cleanUp();
        return;
    }

}

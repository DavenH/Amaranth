#include "EnvelopeMorphSurfaceRasterizer.h"

#include <Curve/Rasterization/Policies/Envelope/EnvelopePolicies.h>

namespace Rasterization {

EnvelopeMorphSurfaceRasterizer::EnvelopeMorphSurfaceRasterizer() {
    auto& request = compatibilityRequest();
    request.lowResCurves = true;
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = PointScalingMode::HalfBipolar;
    request.xMinimum = 0.f;
    request.xMaximum = 10.f;
    rasterizerData.paddingSize = getPaddingSize();
    rasterizerData.wrapsVertices = request.cyclic;
}

void EnvelopeMorphSurfaceRasterizer::renderSurface(
        Mesh* mesh,
        std::vector<Column>& columns,
        int crossSectionResolution,
        int morphAxis,
        const MorphPosition& baseMorph,
        std::optional<Range<float>> timeRange,
        std::vector<EnvelopeSurfaceMarkers>* markers) {
    if (markers != nullptr) {
        markers->assign(columns.size(), {});
    }
    if (columns.empty() || crossSectionResolution <= 0) {
        clearTrilinearOutput();
        return;
    }

    if (mesh == nullptr || mesh->getNumCubes() == 0) {
        clearTrilinearOutput();
        for (Column& column : columns) {
            column.zero();
        }
        return;
    }

    const float sampleStart = timeRange.has_value() ? timeRange->getStart() : 0.f;
    const float sampleInterval = timeRange.has_value()
            ? timeRange->getLength() / float(jmax(1, crossSectionResolution - 1))
            : 1.f / float(crossSectionResolution);
    float inverseSurfaceResolution = columns.size() > 1
                                         ? 1.f / float(columns.size() - 1)
                                         : 0.f;

    const auto* envelope = dynamic_cast<const EnvelopeMesh*>(mesh);
    for (int columnIndex = 0; columnIndex < (int) columns.size(); ++columnIndex) {
        Column& column = columns[columnIndex];
        MorphPosition morph = baseMorph;
        morph[morphAxis].setValueDirect(columnIndex * inverseSurfaceResolution);
        renderCrossSectionAt(*mesh, morph);
        if (markers != nullptr) {
            const auto& intercepts = result().intercepts;
            const auto indices = EnvelopeMarkerPolicy().evaluate(intercepts, envelope, 1);
            auto& section = (*markers)[(size_t) columnIndex];
            if (indices.loopIndex >= 0) {
                section.loopStart = intercepts[(size_t) indices.loopIndex].x;
            }
            if (result().sampleable && !intercepts.empty()) {
                section.signalStart = intercepts.front().x;
                section.signalEnd = intercepts.back().x;
            }
            if (EnvelopePlaybackPolicy().hasReleaseCurve(intercepts, indices.sustainIndex)) {
                section.releaseStart = intercepts[(size_t) indices.sustainIndex].x;
            }
        }

        auto waveformSampler = sampler();
        if (waveformSampler.isSampleable()) {
            waveformSampler.samplePerfectly(sampleInterval, column, sampleStart);
        } else {
            column.zero();
        }
    }

    setMorphPosition(baseMorph);
}

void EnvelopeMorphSurfaceRasterizer::cleanUp() {
    cleanTrilinearRasterization();
}

void EnvelopeMorphSurfaceRasterizer::renderCrossSectionAt(
        Mesh& mesh,
        const MorphPosition& morph) {
    setMesh(&mesh);
    setMorphPosition(morph);
    renderTrilinearWaveform(0.f);
}

}

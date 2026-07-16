#include "EnvelopeMorphSurfaceRasterizer.h"

namespace Rasterization {

EnvelopeMorphSurfaceRasterizer::EnvelopeMorphSurfaceRasterizer() {
    auto& request = getRequest();
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
        const MorphPosition& baseMorph) {
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

    float inverseCrossSectionResolution = 1.f / float(crossSectionResolution);
    float inverseSurfaceResolution = columns.size() > 1
                                         ? 1.f / float(columns.size() - 1)
                                         : 0.f;

    for (int columnIndex = 0; columnIndex < (int) columns.size(); ++columnIndex) {
        Column& column = columns[columnIndex];
        MorphPosition morph = baseMorph;
        morph[morphAxis].setValueDirect(columnIndex * inverseSurfaceResolution);
        renderCrossSectionAt(*mesh, morph);

        auto waveformSampler = sampler();
        if (waveformSampler.isSampleable()) {
            waveformSampler.samplePerfectly(inverseCrossSectionResolution, column, 0.f);
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

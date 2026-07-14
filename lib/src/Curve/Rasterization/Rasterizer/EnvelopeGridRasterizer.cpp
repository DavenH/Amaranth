#include "EnvelopeGridRasterizer.h"

namespace Rasterization {

EnvelopeGridRasterizer::EnvelopeGridRasterizer() {
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

void EnvelopeGridRasterizer::renderGrid(
        Mesh* mesh,
        std::vector<Column>& columns,
        int envelopeResolution,
        int dependentAxis) {
    if (columns.empty() || envelopeResolution <= 0) {
        cleanUp();
        return;
    }

    float inverseColumnResolution = 1.f / float(envelopeResolution);
    float inverseGridResolution = columns.size() > 1
                                      ? 1.f / float(columns.size() - 1)
                                      : 0.f;

    for (int columnIndex = 0; columnIndex < (int) columns.size(); ++columnIndex) {
        Column& column = columns[columnIndex];
        getMorphPosition()[dependentAxis].setValueDirect(columnIndex * inverseGridResolution);
        renderMesh(mesh);

        auto waveformSampler = sampler();
        if (waveformSampler.isSampleable()) {
            waveformSampler.samplePerfectly(inverseColumnResolution, column, 0.f);
        } else {
            column.zero();
        }
    }
}

void EnvelopeGridRasterizer::cleanUp() {
    cleanTrilinearRasterization();
}

void EnvelopeGridRasterizer::renderMesh(Mesh* mesh) {
    if (mesh == nullptr || mesh->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    setMesh(mesh);
    renderTrilinearWaveform(0.f);
    publishTrilinearSnapshot();
}

}

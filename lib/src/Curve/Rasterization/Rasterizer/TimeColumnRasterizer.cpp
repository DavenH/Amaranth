#include <Util/Arithmetic.h>

#include "TimeColumnRenderer.h"

namespace Rasterization {

void TimeColumnRasterizer::render(Context context) const {
    jassert(context.layers != nullptr);
    jassert(context.rasterizer != nullptr);
    jassert(context.columns != nullptr);

    int nextPow2 = context.sumBuffer.size();
    double delta = nextPow2 > 0 ? 1.0 / double(nextPow2) : 0.0;

    for (int colIdx = 0; colIdx < context.numColumns; ++colIdx) {
        int timeColIdx = jmin(context.zoomProgress.size() - 1, colIdx * context.timeIncrement);
        Column& column = (*context.columns)[colIdx];

        if (context.primeDimension == Vertex::Red) {
            nextPow2 = column.size();
            delta = 1.0 / double(nextPow2);
        }

        context.rasterizer->getMorphPosition()[context.primeDimension].setValueDirect(
                context.zoomProgress[timeColIdx]);

        context.sumBuffer.zero();

        for (int layerIdx = 0; layerIdx < (int) context.layers->size(); ++layerIdx) {
            Buffer<float> localBuffer = context.numActiveLayers == 1
                                            ? context.sumBuffer.withSize(nextPow2)
                                            : Buffer<float>(
                                                      context.layerBuffer + layerIdx * nextPow2,
                                                      nextPow2);

            const Layer& layer = (*context.layers)[layerIdx];
            if (!layer.active || layer.mesh == nullptr || !layer.mesh->hasEnoughCubesForCrossSection()) {
                continue;
            }

            float scratchTime = context.primeDimension != Vertex::Time
                                    ? context.panelTime
                                    : context.zoomProgress[timeColIdx];

            if (context.primeDimension == Vertex::Time
                    && context.useScratchTime
                    && context.resolveScratchTime) {
                context.resolveScratchTime(
                        layerIdx,
                        timeColIdx,
                        context.zoomProgress[timeColIdx],
                        scratchTime);
            }

            context.rasterizer->setNoiseSeed(noiseSeedForColumnLayer(colIdx, layerIdx));
            context.rasterizer->setYellow(scratchTime);
            context.rasterizer->renderWaveformOnly(layer.mesh, 0.f);

            auto sampler = context.rasterizer->sampler();
            if (!sampler.isSampleable()) {
                localBuffer.zero();
                continue;
            }

            sampler.sampleWithInterval(localBuffer, delta, 0.0);
            localBuffer.mul(Arithmetic::getRelativePan(layer.pan, context.panelPan));

            if (context.numActiveLayers > 1) {
                context.sumBuffer.add(localBuffer);
            }
        }

        context.sumBuffer.copyTo(column);
    }
}

}

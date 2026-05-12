#pragma once

#include <functional>

#include <App/MeshLibrary.h>
#include <Array/Buffer.h>
#include <Array/Column.h>
#include <Util/Arithmetic.h>

#include "../../Curve/GraphicRasterizer.h"
#include "../../Util/CycleEnums.h"

namespace Cycle::Rasterization {
    class TimeColumnRasterizer {
    public:
        using ScratchTimeResolver = std::function<bool(MeshLibrary::Properties*, int, float, float&)>;

        static int noiseSeedForColumnLayer(int columnIndex, int layerIndex) {
            return columnIndex * 6197 + layerIndex * 104729;
        }

        struct Context {
            MeshLibrary::LayerGroup* timeGroup {};
            TimeRasterizer* rasterizer {};
            std::vector<Column>* columns {};
            Buffer<float> zoomProgress;
            Buffer<float> layerBuffer;
            Buffer<float> sumBuffer;
            ScratchTimeResolver resolveScratchTime;

            int numColumns {};
            int numActiveLayers {};
            int timeIncrement { 1 };
            int primeDimension { Vertex::Time };
            int viewStage {};

            float panelTime {};
            double panelPan { 0.5 };
        };

        void render(Context context) const {
            jassert(context.timeGroup != nullptr);
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

                for (int layerIdx = 0; layerIdx < context.timeGroup->size(); ++layerIdx) {
                    Buffer<float> localBuffer = context.numActiveLayers == 1
                                                    ? context.sumBuffer.withSize(nextPow2)
                                                    : Buffer(context.layerBuffer + layerIdx * nextPow2, nextPow2);

                    MeshLibrary::Layer& layer = context.timeGroup->layers[layerIdx];
                    Mesh* mesh = layer.mesh;
                    MeshLibrary::Properties* props = layer.props;

                    if (!props->active || !mesh->hasEnoughCubesForCrossSection()) {
                        continue;
                    }

                    float scratchTime = context.primeDimension != Vertex::Time
                                            ? context.panelTime
                                            : context.zoomProgress[timeColIdx];

                    if (context.primeDimension == Vertex::Time
                            && context.viewStage >= ViewStages::PostEnvelopes
                            && context.resolveScratchTime) {
                        context.resolveScratchTime(props, timeColIdx, context.zoomProgress[timeColIdx], scratchTime);
                    }

                    context.rasterizer->setNoiseSeed(noiseSeedForColumnLayer(colIdx, layerIdx));
                    context.rasterizer->setYellow(scratchTime);
                    context.rasterizer->updateWaveform(mesh, 0.f);

                    auto sampler = context.rasterizer->sampler();
                    if (!sampler.isSampleable()) {
                        localBuffer.zero();
                        continue;
                    }

                    sampler.sampleWithInterval(localBuffer, delta, 0.0);

                    float relativePan = Arithmetic::getRelativePan(props->pan, context.panelPan);
                    localBuffer.mul(relativePan);

                    if (context.numActiveLayers > 1) {
                        context.sumBuffer.add(localBuffer);
                    }
                }

                context.sumBuffer.copyTo(column);
            }
        }
    };
}

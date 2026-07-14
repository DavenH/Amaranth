#pragma once

#include <functional>
#include <vector>

#include <Array/Buffer.h>
#include <Array/Column.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Rasterizer/GraphicMeshRasterizer.h>

namespace Rasterization {

class TimeColumnRasterizer {
public:
    struct Layer {
        Mesh* mesh {};
        bool active {};
        float pan {};
    };

    using ScratchTimeResolver = std::function<bool(int, int, float, float&)>;

    struct Context {
        const std::vector<Layer>* layers {};
        GraphicRasterizer* rasterizer {};
        std::vector<Column>* columns {};
        Buffer<float> zoomProgress;
        Buffer<float> layerBuffer;
        Buffer<float> sumBuffer;
        ScratchTimeResolver resolveScratchTime;

        int numColumns {};
        int numActiveLayers {};
        int timeIncrement { 1 };
        int primeDimension { Vertex::Time };

        float panelTime {};
        float panelPan { 0.5f };
        bool useScratchTime {};
    };

    static int noiseSeedForLayer(int layerIndex) {
        return layerIndex * 104729;
    }

    static int noiseSeedForColumnLayer(int columnIndex, int layerIndex) {
        return columnIndex * 6197 + noiseSeedForLayer(layerIndex);
    }

    void render(Context context) const;
};

}

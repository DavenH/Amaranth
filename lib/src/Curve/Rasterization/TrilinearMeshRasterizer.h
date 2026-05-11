#pragma once

#include "BaseRasterizer.h"
#include "MeshWaveformRasterizer.h"
#include "../Mesh.h"

namespace Rasterization {
    class TrilinearMeshRasterizer : public BaseRasterizer {
    public:
        SamplerView samplerView() const override {
            return rasterizer.samplerView();
        }

        void setMesh(Mesh* mesh) {
            this->mesh = mesh;
        }

        bool canRasterizeWaveform() const {
            return mesh != nullptr && mesh->hasEnoughCubesForCrossSection();
        }

        MorphPosition& getMorphPosition() {
            return rasterizer.getRequest().morph;
        }

        void setBlue(float blue) {
            rasterizer.getRequest().morph.blue.setValueDirect(blue);
        }

        void setMorphPosition(const MorphPosition& morph) {
            rasterizer.getRequest().morph = morph;
        }

        void setRed(float red) {
            rasterizer.getRequest().morph.red.setValueDirect(red);
        }

        void setYellow(float yellow) {
            rasterizer.getRequest().morph.time.setValueDirect(yellow);
        }

    protected:
        void cleanTrilinearRasterization() {
            rasterizer.clean();
            publishTrilinearSnapshot();
        }

        void publishTrilinearSnapshot() {
            publishSnapshot(rasterizer.createSnapshotSource());
        }

        Mesh* mesh {};
        MeshWaveformRasterizer rasterizer;
    };
}

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

        void updateGeometry() override {
            updateTrilinearGeometry(0.f);
        }

        void updateWaveform() override {
            updateTrilinearWaveform(0.f);
        }

        void updateGeometry(Mesh* mesh, float oscPhase = 0.f) {
            setMesh(mesh);
            updateTrilinearGeometry(oscPhase);
        }

        void updateWaveform(Mesh* mesh, float oscPhase = 0.f) {
            setMesh(mesh);
            updateTrilinearWaveform(oscPhase);
        }

        void setMesh(Mesh* mesh) {
            this->mesh = mesh;
        }

        bool canRasterizeWaveform() const {
            return mesh != nullptr && mesh->hasEnoughCubesForCrossSection();
        }

        bool doesCalcDepthDimensions() const {
            return rasterizer.getRequest().calcDepthDimensions;
        }

        bool doesIntegralSampling() const {
            return rasterizer.getRequest().integralSampling;
        }

        MorphPosition& getMorphPosition() {
            return rasterizer.getRequest().morph;
        }

        void setBlue(float blue) {
            rasterizer.getRequest().morph.blue.setValueDirect(blue);
        }

        void setCalcDepthDimensions(bool calc) {
            rasterizer.getRequest().calcDepthDimensions = calc;
        }

        void setDims(const Dimensions& dims) {
            rasterizer.getRequest().dims = dims;
        }

        void setGuideCurveProvider(GuideCurveProvider* provider) {
            rasterizer.setGuideCurveProvider(provider);
        }

        void setIntegralSampling(bool does) {
            rasterizer.getRequest().integralSampling = does;
        }

        void setInterceptPadding(float value) {
            rasterizer.getRequest().interceptPadding = value;
        }

        void setMorphPosition(const MorphPosition& morph) {
            rasterizer.getRequest().morph = morph;
        }

        void setNoiseSeed(int seed) {
            rasterizer.getRequest().noiseSeed = seed;
        }

        void setRed(float red) {
            rasterizer.getRequest().morph.red.setValueDirect(red);
        }

        void setWrapsEnds(bool wraps) {
            rasterizer.getRequest().cyclic = wraps;
            rasterizerData.wrapsVertices = wraps;
        }

        void setYellow(float yellow) {
            rasterizer.getRequest().morph.time.setValueDirect(yellow);
        }

        void updateOffsetSeeds(int layerSize, int tableSize) {
            rasterizer.updateOffsetSeeds(layerSize, tableSize);
        }

    protected:
        void cleanTrilinearRasterization() {
            rasterizer.clean();
            publishTrilinearSnapshot();
        }

        void publishTrilinearSnapshot() {
            publishSnapshot(rasterizer.createSnapshotSource());
        }

        void updateTrilinearGeometry(float oscPhase) {
            if (mesh == nullptr || mesh->getNumCubes() == 0) {
                cleanTrilinearRasterization();
                return;
            }

            rasterizer.updateGeometry(mesh, oscPhase);
            publishTrilinearSnapshot();
        }

        void updateTrilinearWaveform(float oscPhase) {
            if (mesh == nullptr || mesh->getNumCubes() == 0) {
                cleanTrilinearRasterization();
                return;
            }

            rasterizer.updateWaveform(mesh, oscPhase);
            publishTrilinearSnapshot();
        }

        Mesh* mesh {};
        MeshWaveformRasterizer rasterizer;
    };
}

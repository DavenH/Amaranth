#pragma once

#include <vector>

#include "Builders/RasterizerSnapshotBuilder.h"
#include "GuideCurveOffsetSeeds.h"
#include "Interpolation/TrilinearMeshSlicer.h"
#include "Pipelines/MeshSlicePipeline.h"
#include "Pipelines/PointListRasterizationPipeline.h"
#include "Policies/Mesh/GuideCurvePolicy.h"
#include "Sampling/WaveformSampler.h"
#include "Sources/MeshCubeSource.h"

class GuideCurveProvider;
class Mesh;

namespace Rasterization {
    class MeshWaveformRasterizer {
    public:
        RasterizationRequest& getRequest() { return request; }
        const RasterizationRequest& getRequest() const { return request; }

        void setGuideCurveProvider(GuideCurveProvider* provider) {
            guideCurveProvider = provider;
        }

        GuideCurveProvider* getGuideCurveProvider() const {
            return guideCurveProvider;
        }

        void updateOffsetSeeds(int layerSize, int tableSize) {
            Random random(Time::currentTimeMillis());
            guideCurveOffsetSeeds.randomize(layerSize, tableSize, random);
        }

        void render(Mesh* mesh, float oscPhase = 0.f) {
            if (mesh == nullptr || mesh->getNumCubes() == 0) {
                clean();
                return;
            }

            bool needsResorting = false;

            GuideCurveApplier guideApplier = createGuideCurveApplier(reduction, &needsResorting);
            const RenderResult& meshOutput = meshPipeline.renderWithReduction(
                    MeshCubeSource(mesh),
                    TrilinearMeshSlicer(),
                    request,
                    oscPhase,
                    guideApplier,
                    reduction);

            meshIntercepts = meshOutput.intercepts;
            meshColorPoints = meshOutput.colorPoints;
            waveformOutput = &waveformPipeline.render(
                    meshIntercepts,
                    request,
                    guideCurveProvider,
                    &guideCurveOffsetSeeds);
        }

        void clean() {
            meshIntercepts.clear();
            meshColorPoints.clear();
            waveformOutput = nullptr;
        }

        bool isSampleable() const {
            return waveformOutput != nullptr && waveformOutput->sampleable;
        }

        bool isSampleableAt(float x) const {
            return isSampleable() && WaveformSampler::isSampleableAt(waveform(), x);
        }

        float sampleAt(double angle) const {
            return WaveformSampler::sampleAt(waveform(), !isSampleable(), angle);
        }

        float sampleAt(double angle, int& currentIndex) const {
            return WaveformSampler::sampleAt(waveform(), !isSampleable(), angle, currentIndex);
        }

        void sampleAtIntervals(Buffer<float> intervals, Buffer<float> dest) const {
            if (!isSampleable()) {
                dest.set(0.5f);
                return;
            }

            WaveformSampler::sampleAtIntervals(waveform(), intervals, dest);
        }

        float samplePerfectly(double delta, Buffer<float> buffer, double phase) const {
            return WaveformSampler::samplePerfectly(waveform(), delta, buffer, phase);
        }

        template<typename T>
        T sampleWithInterval(Buffer<float> buffer, T delta, T phase) const {
            return WaveformSampler::sampleWithInterval(waveform(), buffer, delta, phase);
        }

        int getPaddingSize() const {
            return waveformOutput != nullptr ? waveformOutput->paddingSize : request.paddingSize;
        }

        WaveformBuffers waveform() const {
            return waveformOutput != nullptr ? waveformOutput->waveform : WaveformBuffers();
        }

        RasterizerSnapshotSource createSnapshotSource() const {
            RasterizerSnapshotSource source;
            source.intercepts = &meshIntercepts;
            source.colorPoints = &meshColorPoints;

            if (waveformOutput != nullptr) {
                source.curves = &waveformOutput->curves;
                source.waveform = waveformOutput->waveform;
            }

            return source;
        }

        GuideCurveApplier createGuideCurveApplier(
                VertCube::ReductionData& reductionData,
                bool* needsResorting) const {
            GuideCurvePolicyContext context;
            context.guideCurveProvider = guideCurveProvider;
            context.reduction = &reductionData;
            context.scalingMode = request.scalingMode;
            context.cyclic = request.cyclic;
            context.needsResorting = needsResorting;
            context.noiseSeed = request.noiseSeed;
            context.offsetSeeds = &guideCurveOffsetSeeds;

            return GuideCurveApplier(context);
        }

        const RenderResult* getWaveformOutput() const {
            return waveformOutput;
        }

        const std::vector<Intercept>& getIntercepts() const {
            return meshIntercepts;
        }

    private:
        GuideCurveProvider* guideCurveProvider {};

        RasterizationRequest request;
        GuideCurveOffsetSeeds guideCurveOffsetSeeds;
        MeshSlicePipeline meshPipeline;
        PointListRasterizationPipeline waveformPipeline;
        RenderResult const* waveformOutput {};

        std::vector<Intercept> meshIntercepts;
        std::vector<ColorPoint> meshColorPoints;
        VertCube::ReductionData reduction;
    };
}

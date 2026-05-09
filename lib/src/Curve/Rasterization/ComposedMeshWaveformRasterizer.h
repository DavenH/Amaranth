#pragma once

#include <vector>

#include "GuideCurveOffsetSeeds.h"
#include "Policies/Core/SnapshotPolicy.h"
#include "Pipelines/PointListRasterizationPipeline.h"
#include "Policies/Mesh/GuideCurvePolicy.h"
#include "RasterizerComposer.h"
#include "Sampling/WaveformSampler.h"

class GuideCurveProvider;
class Mesh;

namespace Rasterization {
    class ComposedMeshWaveformRasterizer {
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

            auto rasterizer = RasterizerComposer::mesh()
                    .withSource(MeshCubeSource(mesh))
                    .withSlicer(TrilinearMeshSlicer())
                    .withRequest(request)
                    .build();

            bool needsResorting = false;

            GuideCurveApplier guideApplier = createGuideCurveApplier(reduction, &needsResorting);
            meshOutput = rasterizer.renderWithReduction(oscPhase, guideApplier, reduction);

            meshIntercepts = meshOutput.intercepts;
            waveformOutput = &waveformPipeline.render(meshIntercepts, request);
        }

        void clean() {
            meshIntercepts.clear();
            meshOutput = MeshSlicePipeline::Output();
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
            source.colorPoints = &meshOutput.colorPoints;

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

        const MeshSlicePipeline::Output& getMeshOutput() const {
            return meshOutput;
        }

        const PointListRasterizationPipeline::Output* getWaveformOutput() const {
            return waveformOutput;
        }

        const std::vector<Intercept>& getIntercepts() const {
            return meshIntercepts;
        }

    private:
        GuideCurveProvider* guideCurveProvider {};

        RasterizationRequest request;
        GuideCurveOffsetSeeds guideCurveOffsetSeeds;
        PointListRasterizationPipeline waveformPipeline;
        PointListRasterizationPipeline::Output const* waveformOutput {};
        MeshSlicePipeline::Output meshOutput;

        std::vector<Intercept> meshIntercepts;
        VertCube::ReductionData reduction;
    };
}

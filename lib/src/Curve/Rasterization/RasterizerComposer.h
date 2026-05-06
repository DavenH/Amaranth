#pragma once

#include <vector>

#include "Interpolation/TrilinearMeshSlicer.h"
#include "Pipelines/FxRasterizationPipeline.h"
#include "Pipelines/MeshSlicePipeline.h"
#include "Pipelines/PointListRasterizationPipeline.h"
#include "Sources/MeshCubeSource.h"
#include "Sources/PointListSource.h"
#include "Sources/VertexListSource.h"

namespace Rasterization {
    class ComposedPointListRasterizer {
    public:
        ComposedPointListRasterizer(std::vector<Intercept>* points, RasterizationRequest request) :
                points(points)
            ,   request(request) {
        }

        const PointListRasterizationPipeline::Output& render() {
            jassert(points != nullptr);
            return pipeline.render(*points, request);
        }

        const RasterizationRequest& getRequest() const { return request; }

    private:
        std::vector<Intercept>* points {};
        RasterizationRequest request;
        PointListRasterizationPipeline pipeline;
    };

    class PointListComposer {
    public:
        PointListComposer& withSource(std::vector<Intercept>& sourcePoints) {
            points = &sourcePoints;
            return *this;
        }

        PointListComposer& withCyclicPadding(float interceptPadding = 0.f) {
            request.cyclic = true;
            request.interceptPadding = interceptPadding;
            return *this;
        }

        PointListComposer& withNonCyclicPadding(float minimumX = 0.f, float maximumX = 1.f) {
            request.cyclic = false;
            request.xMinimum = minimumX;
            request.xMaximum = maximumX;
            return *this;
        }

        PointListComposer& withRequest(const RasterizationRequest& newRequest) {
            request = newRequest;
            return *this;
        }

        ComposedPointListRasterizer build() const {
            jassert(points != nullptr);
            return ComposedPointListRasterizer(points, request);
        }

    private:
        std::vector<Intercept>* points {};
        RasterizationRequest request;
    };

    class ComposedFxRasterizer {
    public:
        ComposedFxRasterizer(VertexListSource source, RasterizationRequest request) :
                source(source)
            ,   request(request) {
        }

        const FxRasterizationPipeline::Output& render() {
            return pipeline.render(source, request);
        }

        const RasterizationRequest& getRequest() const { return request; }
        const VertexListSource& getSource() const { return source; }

    private:
        VertexListSource source;
        RasterizationRequest request;
        FxRasterizationPipeline pipeline;
    };

    class FxComposer {
    public:
        FxComposer& withSource(VertexListSource newSource) {
            source = newSource;
            return *this;
        }

        FxComposer& withVertices(std::vector<Vertex*>* vertices) {
            source.setVertices(vertices);
            return *this;
        }

        FxComposer& withRequest(const RasterizationRequest& newRequest) {
            request = newRequest;
            request.cyclic = false;
            request.calcDepthDimensions = false;
            return *this;
        }

        FxComposer& withScaling(PointScalingMode scalingMode) {
            request.scalingMode = scalingMode;
            return *this;
        }

        ComposedFxRasterizer build() const {
            return ComposedFxRasterizer(source, request);
        }

    private:
        VertexListSource source;
        RasterizationRequest request;
    };

    class MeshComposer {
    public:
        MeshComposer& withSource(MeshCubeSource newSource) {
            source = newSource;
            return *this;
        }

        MeshComposer& withSlicer(TrilinearMeshSlicer newSlicer) {
            slicer = newSlicer;
            return *this;
        }

        MeshComposer& withRequest(const RasterizationRequest& newRequest) {
            request = newRequest;
            return *this;
        }

        const MeshCubeSource& getSource() const { return source; }
        const RasterizationRequest& getRequest() const { return request; }

    private:
        MeshCubeSource source;
        TrilinearMeshSlicer slicer;
        RasterizationRequest request;
    };

    class RasterizerComposer {
    public:
        static PointListComposer pointList() { return PointListComposer(); }
        static FxComposer fx() { return FxComposer(); }
        static MeshComposer mesh() { return MeshComposer(); }
    };
}

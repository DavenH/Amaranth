#include <Array/Buffer.h>
#include <Algo/FFT.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Obj/MorphPosition.h>

#include "PreviewProcessorFactories.h"

#include "../Nodes/Trimesh/PreparedTrimeshTopology.h"
#include "../Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshGridwiseDsp.h"

#include <algorithm>

namespace CycleV2 {

namespace {

int primaryAxisFromParameter(const String& axisName) {
    if (axisName == "red") {
        return Vertex::Red;
    }

    if (axisName == "blue") {
        return Vertex::Blue;
    }

    return Vertex::Time;
}

MorphPosition meshMorphFromParameters(const std::vector<NodeParameter>& parameters) {
    return {
            typedParameterFloat(parameters, "yellow", 0.5f),
            typedParameterFloat(parameters, "red", 0.5f),
            typedParameterFloat(parameters, "blue", 0.5f)
    };
}

PortDomain primaryOutputDomain(const std::vector<PreviewOutputPort>& outputPorts) {
    if (outputPorts.empty()) {
        return PortDomain::TimeSignal;
    }

    return outputPorts.front().domain;
}

template<typename Values>
void normalizeBipolarValues(Values& values) {
    if (values.empty()) {
        return;
    }

    Buffer<float>(values.data(), (int) values.size())
            .mul(0.5f)
            .add(0.5f)
            .clip(0.f, 1.f);
}

class TrimeshPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::MeshSurface; }

    void render(PreviewProcessContext& context) override {
        if (context.pointCount == 0) {
            context.primary.clear();
            context.secondary.clear();
            return;
        }

        if (reuseCapturedTraversal(context)) {
            return;
        }

        const auto configuration = context.configuration != nullptr
                ? std::dynamic_pointer_cast<const TrimeshConfiguration>(
                        context.configuration->value)
                : nullptr;
        Mesh* mesh = configuration != nullptr
                ? const_cast<Mesh*>(configuration->mesh.get())
                : &fallbackTopology.mesh();
        const MorphPosition morph = configuration != nullptr
                ? configuration->morph
                : meshMorphFromParameters(context.parameters);
        const int primaryAxis = configuration != nullptr
                ? configuration->primaryViewAxis
                : primaryAxisFromParameter(typedParameterString(
                        context.parameters,
                        "primaryAxis",
                        "yellow"));
        const PortDomain outputDomain = primaryOutputDomain(context.outputPorts);
        const bool cyclic = outputDomain == PortDomain::TimeSignal;
        const size_t columnCount = std::max<size_t>(8, context.pointCount / 2);

        context.domain = outputDomain;
        renderSlice(context, *mesh, morph, primaryAxis, cyclic, outputDomain);
        renderGrid(
                context,
                *mesh,
                morph,
                primaryAxis,
                cyclic,
                outputDomain,
                columnCount);
    }

private:
    bool reuseCapturedTraversal(PreviewProcessContext& context) const {
        const bool spectral = context.capturedOutput != nullptr
                && (context.capturedOutput->domain == PortDomain::SpectralMagnitudeSignal
                        || context.capturedOutput->domain
                                == PortDomain::SpectralPhaseSignal);
        const size_t expectedRows = spectral
                ? (size_t) RealFftFullPolarSpectrum::binCountForBufferSize(
                        (int) context.pointCount)
                : context.pointCount;
        if (context.capturedOutput == nullptr
                || !context.capturedOutput->traversalGrid.isValid()
                || context.capturedOutput->traversalGrid.rows != expectedRows) {
            return false;
        }

        const auto& grid = context.capturedOutput->traversalGrid.values;
        const auto& samples = context.capturedOutput->block.samples;
        context.primary.assign(grid.begin(), grid.end());
        context.secondary.assign(samples.begin(), samples.end());
        normalizeBipolarValues(context.primary);
        normalizeBipolarValues(context.secondary);
        context.gridColumns = context.capturedOutput->traversalGrid.columns;
        context.gridRows = context.capturedOutput->traversalGrid.rows;
        context.domain = context.capturedOutput->traversalGrid.metadata.valueDomain;
        context.reusedCapturedTraversal = true;
        return true;
    }

    static void renderSlice(
            PreviewProcessContext& context,
            Mesh& mesh,
            const MorphPosition& morph,
            int primaryAxis,
            bool cyclic,
            PortDomain outputDomain) {
        TrimeshBlockwiseDsp blockwiseDsp;
        SignalPayload slice;
        blockwiseDsp.prepare(&mesh, morph, primaryAxis, cyclic);
        blockwiseDsp.renderPrepared(
                context.pointCount,
                outputDomain,
                ChannelLayout::LinkedStereo,
                slice);
        normalizeBipolarValues(slice.block.samples);
        context.secondary.assign(slice.block.samples.begin(), slice.block.samples.end());
    }

    static void renderGrid(
            PreviewProcessContext& context,
            Mesh& mesh,
            const MorphPosition& morph,
            int primaryAxis,
            bool cyclic,
            PortDomain outputDomain,
            size_t columnCount) {
        TrimeshGridwiseDsp gridwiseDsp;
        gridwiseDsp.setCyclic(cyclic);
        const auto columns = gridwiseDsp.renderColumns(
                mesh,
                morph,
                primaryAxis,
                columnCount,
                context.pointCount,
                outputDomain,
                ChannelLayout::LinkedStereo);

        context.primary.clear();
        context.primary.reserve(columnCount * context.pointCount);
        context.gridColumns = columnCount;
        context.gridRows = context.pointCount;

        for (auto column : columns) {
            normalizeBipolarValues(column.signal.block.samples);
            context.primary.insert(
                    context.primary.end(),
                    column.signal.block.samples.begin(),
                    column.signal.block.samples.end());
        }
    }

    PreparedTrimeshTopology fallbackTopology { "CycleV2PreviewMesh" };
};

}

std::unique_ptr<NodePreviewProcessor> createTrimeshPreviewProcessor() {
    return std::make_unique<TrimeshPreviewProcessor>();
}

}

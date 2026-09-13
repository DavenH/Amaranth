#include <Array/Buffer.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Obj/MorphPosition.h>

#include "Runtime/PreviewProcessorFactories.h"

#include "Graph/NodeParameterMap.h"
#include "Graph/TrimeshSignalSemantics.h"
#include "Nodes/Trimesh/Model/PreparedTrimeshTopology.h"
#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Nodes/Trimesh/Dsp/TrimeshGridwiseDsp.h"

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
    const NodeParameterMap parameterMap(parameters);
    return {
            parameterMap.floatValue("yellow", 0.5f),
            parameterMap.floatValue("red", 0.5f),
            parameterMap.floatValue("blue", 0.5f)
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

bool requiresDisplayNormalization(PortDomain domain, bool bipolar) {
    return domain != PortDomain::SpectralMagnitudeSignal || bipolar;
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

        const auto configuration = context.configuration != nullptr
                ? std::dynamic_pointer_cast<const TrimeshConfiguration>(
                        context.configuration->value)
                : nullptr;
        const PortDomain outputDomain = primaryOutputDomain(context.outputPorts);
        const bool bipolar = configuration != nullptr
                ? configuration->bipolar
                : TrimeshSignalSemantics::isBipolar(context.parameters);
        if (reuseCapturedTraversal(context, outputDomain, bipolar)) {
            return;
        }

        Mesh* mesh = configuration != nullptr
                ? const_cast<Mesh*>(configuration->mesh.get())
                : &fallbackTopology.mesh();
        const MorphPosition morph = configuration != nullptr
                ? configuration->morph
                : meshMorphFromParameters(context.parameters);
        const int primaryAxis = configuration != nullptr
                ? configuration->primaryViewAxis
                : primaryAxisFromParameter(NodeParameterMap(context.parameters)
                        .stringValue("primaryAxis", "yellow"));
        const bool cyclic = outputDomain == PortDomain::TimeSignal;
        const size_t columnCount = std::max<size_t>(8, context.pointCount / 2);
        GuideCurveProvider* guideProvider = configuration != nullptr
                ? configuration->guideCurveProvider.get()
                : nullptr;
        context.domain = outputDomain;
        renderSlice(
                context,
                *mesh,
                morph,
                primaryAxis,
                cyclic,
                outputDomain,
                bipolar,
                guideProvider);
        renderGrid(
                context,
                *mesh,
                morph,
                primaryAxis,
                cyclic,
                outputDomain,
                columnCount,
                bipolar,
                guideProvider);
    }

private:
    bool reuseCapturedTraversal(
            PreviewProcessContext& context,
            PortDomain outputDomain,
            bool bipolar) const {
        if (context.capturedOutput == nullptr
                || !context.capturedOutput->traversalGrid.isValid()) {
            return false;
        }

        const auto& grid = context.capturedOutput->traversalGrid.values;
        const auto& samples = context.capturedOutput->block.samples;
        context.primary.assign(grid.begin(), grid.end());
        context.secondary.assign(samples.begin(), samples.end());
        if (requiresDisplayNormalization(outputDomain, bipolar)) {
            normalizeBipolarValues(context.secondary);
        }
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
            PortDomain outputDomain,
            bool bipolar,
            GuideCurveProvider* guideProvider) {
        TrimeshBlockwiseDsp blockwiseDsp;
        SignalPayload slice;
        blockwiseDsp.setGuideCurveProvider(guideProvider);
        blockwiseDsp.setBipolar(bipolar);
        blockwiseDsp.setFrequencyMidiNote(context.frequencyMidiNote);
        blockwiseDsp.prepare(&mesh, morph, primaryAxis, cyclic, outputDomain);
        blockwiseDsp.renderPrepared(
                context.pointCount,
                outputDomain,
                ChannelLayout::LinkedStereo,
                slice);
        if (requiresDisplayNormalization(outputDomain, bipolar)) {
            normalizeBipolarValues(slice.block.samples);
        }
        context.secondary.assign(slice.block.samples.begin(), slice.block.samples.end());
    }

    static void renderGrid(
            PreviewProcessContext& context,
            Mesh& mesh,
            const MorphPosition& morph,
            int primaryAxis,
            bool cyclic,
            PortDomain outputDomain,
            size_t columnCount,
            bool bipolar,
            GuideCurveProvider* guideProvider) {
        TrimeshGridwiseDsp gridwiseDsp;
        gridwiseDsp.setCyclic(cyclic);
        gridwiseDsp.setGuideCurveProvider(guideProvider);
        gridwiseDsp.setBipolar(bipolar);
        gridwiseDsp.setFrequencyMidiNote(context.frequencyMidiNote);
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

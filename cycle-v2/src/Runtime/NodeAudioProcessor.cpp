#include <Array/Buffer.h>

#include "NodeAudioProcessor.h"
#include "../Nodes/FFT/FftBlockwiseDsp.h"
#include "../Nodes/FFT/FftGridwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../Nodes/Waveshaper/WaveshaperSignalProcessor.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Obj/MorphPosition.h>

namespace CycleV2 {

namespace {

constexpr size_t kDefaultTraversalColumns = 8;

Buffer<float> blockBuffer(AudioProcessBlock& block, size_t frameCount) {
    return { block.samples.data(), (int) frameCount };
}

Buffer<float> payloadBuffer(SignalPayload& payload, size_t frameCount) {
    return blockBuffer(payload.block, frameCount);
}

SignalPayload makeOutputPayload(const AudioOutputPort& port, size_t frameCount) {
    SignalPayload payload;
    payload.domain = port.domain;
    payload.channelLayout = port.channelLayout;
    payload.block.samples.resize(frameCount);
    return payload;
}

SignalPayload makeOutputPayload(AudioProcessContext& context, size_t index) {
    if (index < context.outputPorts.size()) {
        return makeOutputPayload(context.outputPorts[index], context.frameCount);
    }

    SignalPayload payload;
    payload.block.samples.resize(context.frameCount);
    return payload;
}

void publishSingleOutput(AudioProcessContext& context, SignalPayload output) {
    context.outputs.clear();
    context.outputs.push_back(std::move(output));
}

void publishOutputs(AudioProcessContext& context, std::vector<SignalPayload> outputs) {
    context.outputs = std::move(outputs);
}

void clearOutput(AudioProcessContext& context) {
    auto output = makeOutputPayload(context, 0);
    payloadBuffer(output, context.frameCount).zero();
    publishSingleOutput(context, std::move(output));
}

SignalPayload* inputAt(AudioProcessContext& context, size_t index) {
    if (index >= context.inputs.size()) {
        return nullptr;
    }

    const size_t sampleCount = context.inputs[index].block.samples.size();
    if (sampleCount != 1 && sampleCount < context.frameCount) {
        return nullptr;
    }

    return &context.inputs[index];
}

void configureTraversalGrid(SignalTraversalGrid& grid, size_t columns, size_t rows) {
    grid.columns = columns;
    grid.rows = rows;
    grid.values.assign(columns * rows, 0.f);
}

float blockValueAt(const AudioProcessBlock& block, size_t row) {
    if (block.samples.empty()) {
        return 0.f;
    }

    if (block.samples.size() == 1) {
        return block.samples.front();
    }

    return row < block.samples.size() ? block.samples[row] : 0.f;
}

float payloadValueAt(const SignalPayload& payload, size_t row) {
    return blockValueAt(payload.block, row);
}

bool isScalarPayload(const SignalPayload* payload) {
    return payload != nullptr && payload->block.samples.size() == 1 && !payload->traversalGrid.isValid();
}

bool isVectorPayload(const SignalPayload* payload) {
    return payload != nullptr && payload->block.samples.size() > 1 && !payload->traversalGrid.isValid();
}

bool isGridPayload(const SignalPayload* payload) {
    return payload != nullptr && payload->traversalGrid.isValid();
}

PortDomain concreteDomainFor(const SignalPayload* left, const SignalPayload* right, PortDomain fallback) {
    if (left != nullptr && left->domain != PortDomain::ControlSignal && left->domain != PortDomain::EnvelopeSignal) {
        return left->domain;
    }

    if (right != nullptr && right->domain != PortDomain::ControlSignal && right->domain != PortDomain::EnvelopeSignal) {
        return right->domain;
    }

    return fallback;
}

ChannelLayout layoutFor(const SignalPayload* left, const SignalPayload* right, ChannelLayout fallback) {
    if (left != nullptr && left->channelLayout != ChannelLayout::Mono) {
        return left->channelLayout;
    }

    if (right != nullptr && right->channelLayout != ChannelLayout::Mono) {
        return right->channelLayout;
    }

    return fallback;
}

float gridValueAt(const SignalTraversalGrid& grid, size_t column, size_t row) {
    if (!grid.isValid() || column >= grid.columns || row >= grid.rows) {
        return 0.f;
    }

    return grid.values[column * grid.rows + row];
}

void publishVectorAsTraversalGrid(SignalPayload& payload, size_t columns) {
    if (payload.block.samples.empty()) {
        payload.traversalGrid = {};
        return;
    }

    configureTraversalGrid(payload.traversalGrid, columns, payload.block.samples.size());

    for (size_t column = 0; column < columns; ++column) {
        std::copy(
                payload.block.samples.begin(),
                payload.block.samples.end(),
                payload.traversalGrid.values.begin() + (ptrdiff_t) (column * payload.block.samples.size()));
    }
}

void publishGridColumns(SignalPayload& payload, const std::vector<TrimeshGridColumn>& columns) {
    if (columns.empty() || columns.front().signal.block.samples.empty()) {
        payload.traversalGrid = {};
        return;
    }

    const size_t rows = columns.front().signal.block.samples.size();
    configureTraversalGrid(payload.traversalGrid, columns.size(), rows);

    for (size_t column = 0; column < columns.size(); ++column) {
        const auto& samples = columns[column].signal.block.samples;
        const size_t count = std::min(rows, samples.size());
        std::copy(
                samples.begin(),
                samples.begin() + (ptrdiff_t) count,
                payload.traversalGrid.values.begin() + (ptrdiff_t) (column * rows));
    }
}

void copyTraversalGrid(SignalPayload& dest, const SignalTraversalGrid& source) {
    dest.traversalGrid = source;
}

float parameterFloat(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        float fallback) {
    for (const auto& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value.getFloatValue();
        }
    }

    return fallback;
}

String parameterString(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        const String& fallback) {
    for (const auto& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value;
        }
    }

    return fallback;
}

bool parameterBool(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        bool fallback) {
    const String value = parameterString(parameters, id, fallback ? "1" : "0").toLowerCase();
    return value == "1" || value == "true" || value == "on" || value == "yes";
}

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
            parameterFloat(parameters, "yellow", 0.5f),
            parameterFloat(parameters, "red", 0.5f),
            parameterFloat(parameters, "blue", 0.5f)
    };
}

}

class FixedRoleProcessor final : public NodeAudioProcessor {
public:
    explicit FixedRoleProcessor(AudioModuleRole roleToUse) :
            processorRole(roleToUse)
        ,   defaultMesh(TrimeshMeshFactory::createDefaultMesh()) {}

    ~FixedRoleProcessor() override {
        if (defaultMesh != nullptr) {
            defaultMesh->destroy();
        }
    }

    AudioModuleRole role() const override { return processorRole; }

    void process(AudioProcessContext& context) override {
        switch (processorRole) {
            case AudioModuleRole::WaveSource:
            case AudioModuleRole::ImageSource:
                processRampSource(context);
                break;

            case AudioModuleRole::MeshSource:
                processMeshSource(context);
                break;

            case AudioModuleRole::Envelope:
                processEnvelope(context);
                break;

            case AudioModuleRole::Fft:
                processFft(context);
                break;

            case AudioModuleRole::Ifft:
                processIfft(context);
                break;

            case AudioModuleRole::StereoSplit:
                processStereoSplit(context);
                break;

            case AudioModuleRole::StereoJoin:
                processStereoJoin(context);
                break;

            case AudioModuleRole::Add:
                processAdd(context);
                break;

            case AudioModuleRole::Multiply:
                processMultiply(context);
                break;

            case AudioModuleRole::Output:
            case AudioModuleRole::ImpulseResponse:
            case AudioModuleRole::Reverb:
            case AudioModuleRole::Delay:
            case AudioModuleRole::GenericProcessor:
                processPassthrough(context);
                break;

            case AudioModuleRole::Waveshaper:
                processWaveshaper(context);
                break;

            case AudioModuleRole::VoiceContext:
            case AudioModuleRole::GuideCurve:
            case AudioModuleRole::None:
            default:
                clearOutput(context);
                break;
        }
    }

private:
    enum class BinaryGridOperation {
        Add,
        Multiply
    };

    void processRampSource(AudioProcessContext& context) const {
        auto output = makeOutputPayload(context, 0);

        if (context.frameCount == 0) {
            publishSingleOutput(context, std::move(output));
            return;
        }

        const float denominator = context.frameCount > 1 ? (float) (context.frameCount - 1) : 1.f;
        const float level = parameterFloat(context.parameters, "level", 1.f);

        payloadBuffer(output, context.frameCount).ramp(0.f, level / denominator);
        publishVectorAsTraversalGrid(output, kDefaultTraversalColumns);
        publishSingleOutput(context, std::move(output));
    }

    void processMeshSource(AudioProcessContext& context) const {
        AudioOutputPort outputPort;
        if (!context.outputPorts.empty()) {
            outputPort = context.outputPorts.front();
        } else {
            outputPort = { "out", PortDomain::ControlSignal, ChannelLayout::LinkedStereo };
        }

        auto output = makeOutputPayload(outputPort, context.frameCount);

        if (context.frameCount == 0) {
            publishSingleOutput(context, std::move(output));
            return;
        }

        const auto morph = meshMorphFromParameters(context.parameters);
        const int primaryAxis = primaryAxisFromParameter(
                parameterString(context.parameters, "primaryAxis", "yellow"));

        trimeshDsp.setMesh(defaultMesh.get());
        trimeshDsp.setMorphPosition(morph);
        trimeshDsp.setPrimaryViewAxis(primaryAxis);
        trimeshDsp.setCyclic(outputPort.domain == PortDomain::TimeSignal);
        trimeshDsp.renderCycle(context.frameCount, outputPort.domain, outputPort.channelLayout, output);
        publishGridColumns(
                output,
                renderMeshColumns(
                        morph,
                        primaryAxis,
                        std::max(kDefaultTraversalColumns, context.frameCount / 2),
                        context.frameCount,
                        outputPort.domain,
                        outputPort.channelLayout));
        publishSingleOutput(context, std::move(output));
    }

    void processEnvelope(AudioProcessContext& context) const {
        auto output = makeOutputPayload(context, 0);
        const float level = parameterFloat(context.parameters, "level", 1.f);

        payloadBuffer(output, context.frameCount).set(level);
        publishVectorAsTraversalGrid(output, kDefaultTraversalColumns);
        publishSingleOutput(context, std::move(output));
    }

    void processFft(AudioProcessContext& context) const {
        SignalPayload* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        auto magnitude = makeOutputPayload(context, 0);
        auto phase = makeOutputPayload(context, 1);
        fftDsp.forward(input->block, magnitude.block, phase.block);
        publishFftTraversalGrids(*input, magnitude, phase);

        publishOutputs(context, { std::move(magnitude), std::move(phase) });
    }

    void processIfft(AudioProcessContext& context) const {
        SignalPayload* magnitude = inputAt(context, 0);

        if (magnitude == nullptr) {
            clearOutput(context);
            return;
        }

        auto output = makeOutputPayload(context, 0);
        SignalPayload* phase = inputAt(context, 1);
        fftDsp.inverse(magnitude->block, phase != nullptr ? &phase->block : nullptr, output.block);
        publishIfftTraversalGrid(*magnitude, phase, output);
        publishSingleOutput(context, std::move(output));
    }

    void processStereoSplit(AudioProcessContext& context) const {
        SignalPayload* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        auto left = makeOutputPayload(context, 0);
        auto right = makeOutputPayload(context, 1);
        blockBuffer(input->block, context.frameCount).copyTo(payloadBuffer(left, context.frameCount));
        blockBuffer(input->block, context.frameCount).copyTo(payloadBuffer(right, context.frameCount));
        copyTraversalGrid(left, input->traversalGrid);
        copyTraversalGrid(right, input->traversalGrid);

        publishOutputs(context, { std::move(left), std::move(right) });
    }

    void processStereoJoin(AudioProcessContext& context) const {
        processAdd(context);
    }

    void processAdd(AudioProcessContext& context) const {
        auto output = makeOutputPayload(context, 0);
        SignalPayload* left = inputAt(context, 0);
        SignalPayload* right = inputAt(context, 1);

        configureBinaryOutputMetadata(output, left, right);
        applyBinaryBlock(output, left, right, BinaryGridOperation::Add, context.frameCount);
        applyBinaryTraversalGrid(output, left, right, BinaryGridOperation::Add);
        publishSingleOutput(context, std::move(output));
    }

    void processMultiply(AudioProcessContext& context) const {
        auto output = makeOutputPayload(context, 0);
        SignalPayload* left = inputAt(context, 0);
        SignalPayload* right = inputAt(context, 1);

        if (left == nullptr || right == nullptr) {
            clearOutput(context);
            return;
        }

        configureBinaryOutputMetadata(output, left, right);
        applyBinaryBlock(output, left, right, BinaryGridOperation::Multiply, context.frameCount);
        applyBinaryTraversalGrid(output, left, right, BinaryGridOperation::Multiply);
        publishSingleOutput(context, std::move(output));
    }

    void processPassthrough(AudioProcessContext& context) const {
        SignalPayload* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        auto output = makeOutputPayload(context, 0);
        if (context.outputPorts.empty()) {
            output.domain = input->domain;
            output.channelLayout = input->channelLayout;
        }

        blockBuffer(input->block, context.frameCount).copyTo(payloadBuffer(output, context.frameCount));
        copyTraversalGrid(output, input->traversalGrid);
        publishSingleOutput(context, std::move(output));
    }

    void processWaveshaper(AudioProcessContext& context) const {
        SignalPayload* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        if (!parameterBool(context.parameters, "enabled", true)) {
            processPassthrough(context);
            return;
        }

        auto output = makeOutputPayload(context, 0);
        if (context.outputPorts.empty()) {
            output.domain = input->domain;
            output.channelLayout = input->channelLayout;
        }

        waveshaperDsp.process(output, *input, context.parameters, context.frameCount);
        publishSingleOutput(context, std::move(output));
    }

    std::vector<TrimeshGridColumn> renderMeshColumns(
            const MorphPosition& morph,
            int primaryAxis,
            size_t columnCount,
            size_t frameCount,
            PortDomain domain,
            ChannelLayout channelLayout) const {
        TrimeshGridwiseDsp gridwiseDsp;
        gridwiseDsp.setCyclic(domain == PortDomain::TimeSignal);
        return gridwiseDsp.renderColumns(
                *defaultMesh,
                morph,
                primaryAxis,
                columnCount,
                frameCount,
                domain,
                channelLayout);
    }

    void publishFftTraversalGrids(
            const SignalPayload& input,
            SignalPayload& magnitude,
            SignalPayload& phase) const {
        if (!input.traversalGrid.isValid()) {
            return;
        }

        std::vector<AudioProcessBlock> timeColumns;
        timeColumns.reserve(input.traversalGrid.columns);

        for (size_t column = 0; column < input.traversalGrid.columns; ++column) {
            AudioProcessBlock timeColumn;
            timeColumn.samples.assign(
                    input.traversalGrid.values.begin() + (ptrdiff_t) (column * input.traversalGrid.rows),
                    input.traversalGrid.values.begin() + (ptrdiff_t) ((column + 1) * input.traversalGrid.rows));
            timeColumns.push_back(std::move(timeColumn));
        }

        const auto columns = fftGridwiseDsp.forwardColumns(timeColumns);
        publishFftGridResult(columns, magnitude, true);
        publishFftGridResult(columns, phase, false);
    }

    void publishFftGridResult(
            const std::vector<FftGridColumn>& columns,
            SignalPayload& output,
            bool magnitude) const {
        if (columns.empty()) {
            return;
        }

        const auto& first = magnitude ? columns.front().magnitude : columns.front().phase;
        if (first.block.samples.empty()) {
            return;
        }

        configureTraversalGrid(output.traversalGrid, columns.size(), first.block.samples.size());

        for (size_t column = 0; column < columns.size(); ++column) {
            const auto& payload = magnitude ? columns[column].magnitude : columns[column].phase;
            const auto& samples = payload.block.samples;
            const size_t count = std::min(output.traversalGrid.rows, samples.size());
            std::copy(
                    samples.begin(),
                    samples.begin() + (ptrdiff_t) count,
                    output.traversalGrid.values.begin() + (ptrdiff_t) (column * output.traversalGrid.rows));
        }
    }

    void publishIfftTraversalGrid(
            const SignalPayload& magnitude,
            const SignalPayload* phase,
            SignalPayload& output) const {
        if (!magnitude.traversalGrid.isValid()) {
            return;
        }

        configureTraversalGrid(
                output.traversalGrid,
                magnitude.traversalGrid.columns,
                output.block.samples.size());

        for (size_t column = 0; column < magnitude.traversalGrid.columns; ++column) {
            AudioProcessBlock magnitudeColumn;
            AudioProcessBlock phaseColumn;
            AudioProcessBlock outputColumn;
            magnitudeColumn.samples.assign(
                    magnitude.traversalGrid.values.begin() + (ptrdiff_t) (column * magnitude.traversalGrid.rows),
                    magnitude.traversalGrid.values.begin() + (ptrdiff_t) ((column + 1) * magnitude.traversalGrid.rows));
            outputColumn.samples.resize(output.block.samples.size());

            const AudioProcessBlock* phaseColumnPtr = nullptr;
            if (phase != nullptr && phase->traversalGrid.isValid()
                    && phase->traversalGrid.columns == magnitude.traversalGrid.columns) {
                phaseColumn.samples.assign(
                        phase->traversalGrid.values.begin() + (ptrdiff_t) (column * phase->traversalGrid.rows),
                        phase->traversalGrid.values.begin() + (ptrdiff_t) ((column + 1) * phase->traversalGrid.rows));
                phaseColumnPtr = &phaseColumn;
            }

            fftDsp.inverse(magnitudeColumn, phaseColumnPtr, outputColumn);
            const size_t count = std::min(output.traversalGrid.rows, outputColumn.samples.size());
            std::copy(
                    outputColumn.samples.begin(),
                    outputColumn.samples.begin() + (ptrdiff_t) count,
                    output.traversalGrid.values.begin() + (ptrdiff_t) (column * output.traversalGrid.rows));
        }
    }

    void applyBinaryTraversalGrid(
            SignalPayload& output,
            const SignalPayload* left,
            const SignalPayload* right,
            BinaryGridOperation operation) const {
        const SignalTraversalGrid* grid = firstValidGrid(left, right);

        if (grid == nullptr) {
            output.traversalGrid = {};
            return;
        }

        configureTraversalGrid(output.traversalGrid, grid->columns, grid->rows);

        for (size_t column = 0; column < grid->columns; ++column) {
            for (size_t row = 0; row < grid->rows; ++row) {
                const float leftValue = operandValue(left, column, row);
                const float rightValue = operandValue(right, column, row);
                output.traversalGrid.values[column * grid->rows + row] = operation == BinaryGridOperation::Add
                        ? leftValue + rightValue
                        : leftValue * rightValue;
            }
        }
    }

    void applyBinaryBlock(
            SignalPayload& output,
            const SignalPayload* left,
            const SignalPayload* right,
            BinaryGridOperation operation,
            size_t frameCount) const {
        output.block.samples.resize(frameCount);

        for (size_t row = 0; row < frameCount; ++row) {
            const float leftValue = operandBlockValue(left, row, operation, true);
            const float rightValue = operandBlockValue(right, row, operation, false);
            output.block.samples[row] = operation == BinaryGridOperation::Add
                    ? leftValue + rightValue
                    : leftValue * rightValue;
        }
    }

    void configureBinaryOutputMetadata(
            SignalPayload& output,
            const SignalPayload* left,
            const SignalPayload* right) const {
        output.domain = concreteDomainFor(left, right, output.domain);
        output.channelLayout = layoutFor(left, right, output.channelLayout);
    }

    const SignalTraversalGrid* firstValidGrid(
            const SignalPayload* left,
            const SignalPayload* right) const {
        if (left != nullptr && left->traversalGrid.isValid()) {
            return &left->traversalGrid;
        }

        if (right != nullptr && right->traversalGrid.isValid()) {
            return &right->traversalGrid;
        }

        return nullptr;
    }

    float operandValue(const SignalPayload* payload, size_t column, size_t row) const {
        if (payload == nullptr) {
            return 0.f;
        }

        if (payload->traversalGrid.isValid()) {
            return gridValueAt(payload->traversalGrid, column, row);
        }

        return payloadValueAt(*payload, row);
    }

    float operandBlockValue(
            const SignalPayload* payload,
            size_t row,
            BinaryGridOperation operation,
            bool leftOperand) const {
        if (payload != nullptr) {
            return payloadValueAt(*payload, row);
        }

        if (operation == BinaryGridOperation::Multiply) {
            return leftOperand ? 1.f : 0.f;
        }

        return 0.f;
    }

    AudioModuleRole processorRole {};
    mutable FftBlockwiseDsp fftDsp;
    mutable FftGridwiseDsp fftGridwiseDsp;
    mutable TrimeshBlockwiseDsp trimeshDsp;
    mutable WaveshaperSignalProcessor waveshaperDsp;
    std::unique_ptr<Mesh> defaultMesh;
};

void NodeAudioProcessor::prepare(size_t) {}

std::unique_ptr<NodeAudioProcessor> NodeAudioProcessorFactory::create(AudioModuleRole role) const {
    if (role == AudioModuleRole::None) {
        return {};
    }

    return std::make_unique<FixedRoleProcessor>(role);
}

}

#pragma once

#include "AudioProcessContextUtils.h"

#include <Array/Buffer.h>

#include <vector>

namespace CycleV2 {

enum class BinarySignalOperation {
    Add,
    Multiply
};

enum class BinaryGridOperandMode {
    Block,
    MatchingGrid,
    EnvelopeTraversal,
    BroadcastFirstColumn,
    Incompatible
};

class BinarySignalProcessor {
public:
    void process(
            SignalPayload& output,
            const SignalPayload* left,
            const SignalPayload* right,
            BinarySignalOperation operation,
            size_t frameCount,
            const AudioProcessWorkArena* arena = nullptr) {
        configureOutputMetadata(output, left, right);
        processBlock(output, left, right, operation, frameCount);
        processTraversalGrid(output, left, right, operation, arena);
    }

private:
    void processBlock(
            SignalPayload& output,
            const SignalPayload* left,
            const SignalPayload* right,
            BinarySignalOperation operation,
            size_t frameCount) {
        const size_t channelCount = payloadChannelCount(output);
        for (size_t channel = 0; channel < channelCount; ++channel) {
            auto& outputSamples = payloadBlock(output, channel).samples;
            outputSamples.resize(frameCount);
            rightOperand.resize(frameCount);
            prepareOperand(outputSamples, left, frameCount, channel, 0, false);
            prepareOperand(rightOperand, right, frameCount, channel, 0, false);
            applyOperation(outputSamples, rightOperand, operation);
            clampOutputDomain({ outputSamples.data(), (int) outputSamples.size() }, output.domain);
        }
    }

    void processTraversalGrid(
            SignalPayload& output,
            const SignalPayload* left,
            const SignalPayload* right,
            BinarySignalOperation operation,
            const AudioProcessWorkArena* arena) {
        const SignalTraversalGrid* grid = outputGridFor(output.domain, left, right);
        if (grid == nullptr) {
            clearTraversalGrid(output.traversalGrid);
            return;
        }

        const BinaryGridOperandMode leftMode = gridOperandModeFor(left, *grid);
        const BinaryGridOperandMode rightMode = gridOperandModeFor(right, *grid);
        if (leftMode == BinaryGridOperandMode::Incompatible
                || rightMode == BinaryGridOperandMode::Incompatible) {
            clearTraversalGrid(output.traversalGrid);
            return;
        }

        configureTraversalGrid(
                output.traversalGrid,
                grid->columns,
                grid->rows,
                outputMetadataFor(output.domain, *grid),
                arena);

        rightOperand.resize(grid->rows);
        for (size_t column = 0; column < grid->columns; ++column) {
            auto outputColumn = columnBuffer(output.traversalGrid, column);
            prepareGridColumnOperand(outputColumn, left, *grid, column, leftMode);
            prepareGridColumnOperand(rightOperand, right, *grid, column, rightMode);
            applyOperation(outputColumn, rightOperand, operation);
            clampOutputDomain(outputColumn, output.domain);
        }
    }

    static void configureOutputMetadata(
            SignalPayload& output,
            const SignalPayload* left,
            const SignalPayload* right) {
        output.domain = concreteDomainFor(left, right, output.domain);
        output.channelLayout = layoutFor(left, right, output.channelLayout);
    }

    static void prepareOperand(
            std::vector<float>& dest,
            const SignalPayload* payload,
            size_t frameCount,
            size_t channel,
            size_t gridColumn = 0,
            bool useTraversalGrid = true) {
        dest.resize(frameCount);
        prepareOperand(
                { dest.data(), (int) dest.size() },
                payload,
                frameCount,
                channel,
                gridColumn,
                useTraversalGrid);
    }

    static void prepareOperand(
            Buffer<float> dest,
            const SignalPayload* payload,
            size_t frameCount,
            size_t channel,
            size_t gridColumn = 0,
            bool useTraversalGrid = true) {
        if (payload == nullptr) {
            dest.zero();
            return;
        }

        const size_t sourceChannel = payload->isStereo() ? channel : 0;
        const auto& grid = payloadTraversalGrid(*payload, sourceChannel);
        if (useTraversalGrid && grid.isValid()) {
            copyTraversalGridColumn(dest, grid, gridColumn);
            return;
        }

        copyBlockExpandingScalars(dest, payloadBlock(*payload, sourceChannel), frameCount);
    }

    static void prepareGridColumnOperand(
            std::vector<float>& dest,
            const SignalPayload* payload,
            const SignalTraversalGrid& target,
            size_t column,
            BinaryGridOperandMode mode) {
        dest.resize(target.rows);
        prepareGridColumnOperand({ dest.data(), (int) dest.size() }, payload, target, column, mode);
    }

    static void prepareGridColumnOperand(
            Buffer<float> dest,
            const SignalPayload* payload,
            const SignalTraversalGrid& target,
            size_t column,
            BinaryGridOperandMode mode) {
        if (payload == nullptr) {
            dest.zero();
            return;
        }

        if (mode == BinaryGridOperandMode::MatchingGrid) {
            copyTraversalGridColumn(dest, payload->traversalGrid, column);
            return;
        }

        if (mode == BinaryGridOperandMode::EnvelopeTraversal) {
            const SignalTraversalGrid& envelope = payload->traversalGrid;
            const float sourcePosition = target.columns > 0
                    ? (float) column * (float) envelope.columns / (float) target.columns
                    : 0.f;
            const size_t firstColumn = std::min((size_t) sourcePosition, envelope.columns - 1);
            const size_t secondColumn = std::min(firstColumn + 1, envelope.columns - 1);
            const float fraction = sourcePosition - (float) firstColumn;
            const float first = envelope.values[firstColumn * envelope.rows];
            const float second = envelope.values[secondColumn * envelope.rows];
            dest.set(first + fraction * (second - first));
            return;
        }

        if (mode == BinaryGridOperandMode::BroadcastFirstColumn) {
            copyTraversalGridColumn(dest, payload->traversalGrid, 0);
            return;
        }

        if (payload->block.samples.size() == target.columns && column < payload->block.samples.size()) {
            dest.set(payload->block.samples[column]);
            return;
        }

        copyBlockExpandingScalars(dest, payload->block, target.rows);
    }

    static void applyOperation(
            std::vector<float>& output,
            std::vector<float>& right,
            BinarySignalOperation operation) {
        applyOperation({ output.data(), (int) output.size() }, right, operation);
    }

    static void applyOperation(
            Buffer<float> output,
            std::vector<float>& right,
            BinarySignalOperation operation) {
        Buffer<float> rightBuffer(right.data(), (int) right.size());
        if (operation == BinarySignalOperation::Add) {
            output.add(rightBuffer);
            return;
        }

        output.mul(rightBuffer);
    }

    static void clampOutputDomain(Buffer<float> output, PortDomain domain) {
        if (domain == PortDomain::SpectralMagnitudeSignal) {
            output.threshLT(0.f);
        }
    }

    static Buffer<float> columnBuffer(SignalTraversalGrid& grid, size_t column) {
        return { grid.values.data() + column * grid.rows, (int) grid.rows };
    }

    static const SignalTraversalGrid* outputGridFor(
            PortDomain outputDomain,
            const SignalPayload* left,
            const SignalPayload* right) {
        const SignalPayload* concrete = concreteGridPayloadFor(outputDomain, left, right);
        if (concrete != nullptr) {
            return &concrete->traversalGrid;
        }

        if (left != nullptr && left->traversalGrid.isValid()) {
            return &left->traversalGrid;
        }

        if (right != nullptr && right->traversalGrid.isValid()) {
            return &right->traversalGrid;
        }

        return nullptr;
    }

    static const SignalPayload* concreteGridPayloadFor(
            PortDomain outputDomain,
            const SignalPayload* left,
            const SignalPayload* right) {
        if (left != nullptr
                && left->traversalGrid.isValid()
                && left->traversalGrid.metadata.valueDomain == outputDomain) {
            return left;
        }

        if (right != nullptr
                && right->traversalGrid.isValid()
                && right->traversalGrid.metadata.valueDomain == outputDomain) {
            return right;
        }

        if (left != nullptr
                && left->traversalGrid.isValid()
                && isConcreteSignalDomain(left->traversalGrid.metadata.valueDomain)) {
            return left;
        }

        if (right != nullptr
                && right->traversalGrid.isValid()
                && isConcreteSignalDomain(right->traversalGrid.metadata.valueDomain)) {
            return right;
        }

        return nullptr;
    }

    static TraversalGridMetadata outputMetadataFor(PortDomain outputDomain, const SignalTraversalGrid& grid) {
        auto metadata = grid.metadata;
        metadata.valueDomain = outputDomain;
        metadata.arity = traversalGridArity(grid.columns, grid.rows);
        return metadata;
    }

    static BinaryGridOperandMode gridOperandModeFor(
            const SignalPayload* payload,
            const SignalTraversalGrid& target) {
        if (payload == nullptr || !payload->traversalGrid.isValid()) {
            return BinaryGridOperandMode::Block;
        }

        if (traversalGridShapeAndMetadataCompatible(payload->traversalGrid, target)) {
            return BinaryGridOperandMode::MatchingGrid;
        }

        if (isEnvelopeTraversalGrid(payload->traversalGrid, target)) {
            return BinaryGridOperandMode::EnvelopeTraversal;
        }

        if (isRepeatedTimeVector(payload->traversalGrid, target)
                || isSingleColumnVector(payload->traversalGrid, target)) {
            return BinaryGridOperandMode::BroadcastFirstColumn;
        }

        return BinaryGridOperandMode::Incompatible;
    }

    static bool traversalGridShapeAndMetadataCompatible(
            const SignalTraversalGrid& candidate,
            const SignalTraversalGrid& target) {
        return candidate.columns == target.columns
                && candidate.rows == target.rows
                && traversalGridMetadataCompatible(candidate.metadata, target.metadata);
    }

    static bool isRepeatedTimeVector(const SignalTraversalGrid& candidate, const SignalTraversalGrid& target) {
        return candidate.isValid()
                && candidate.rows == target.rows
                && candidate.metadata.valueDomain == PortDomain::EnvelopeSignal
                && candidate.metadata.columnAxis == TraversalGridAxis::Repeated
                && candidate.metadata.rowAxis == TraversalGridAxis::Time
                && target.metadata.rowAxis == TraversalGridAxis::Time;
    }

    static bool isEnvelopeTraversalGrid(
            const SignalTraversalGrid& candidate,
            const SignalTraversalGrid&) {
        return candidate.isValid()
                && candidate.metadata.valueDomain == PortDomain::EnvelopeSignal
                && candidate.metadata.columnAxis == TraversalGridAxis::Time
                && candidate.metadata.rowAxis == TraversalGridAxis::Repeated;
    }

    static bool isSingleColumnVector(const SignalTraversalGrid& candidate, const SignalTraversalGrid& target) {
        return candidate.isValid()
                && candidate.columns == 1
                && candidate.rows == target.rows
                && traversalGridMetadataCompatible(candidate.metadata, target.metadata);
    }

    static PortDomain concreteDomainFor(const SignalPayload* left, const SignalPayload* right, PortDomain fallback) {
        if (left != nullptr && left->domain != PortDomain::ControlSignal && left->domain != PortDomain::EnvelopeSignal) {
            return left->domain;
        }

        if (right != nullptr && right->domain != PortDomain::ControlSignal && right->domain != PortDomain::EnvelopeSignal) {
            return right->domain;
        }

        return fallback;
    }

    static bool isConcreteSignalDomain(PortDomain domain) {
        return domain != PortDomain::ControlSignal
                && domain != PortDomain::EnvelopeSignal
                && domain != PortDomain::DomainContext
                && domain != PortDomain::VoiceControlSignal;
    }

    static ChannelLayout layoutFor(const SignalPayload* left, const SignalPayload* right, ChannelLayout fallback) {
        if (left != nullptr && left->channelLayout != ChannelLayout::Mono) {
            return left->channelLayout;
        }

        if (right != nullptr && right->channelLayout != ChannelLayout::Mono) {
            return right->channelLayout;
        }

        return fallback;
    }

    std::vector<float> rightOperand;
};

}

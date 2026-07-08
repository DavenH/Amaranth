#pragma once

#include "AudioProcessContextUtils.h"

#include <Array/Buffer.h>

#include <vector>

namespace CycleV2 {

enum class BinarySignalOperation {
    Add,
    Multiply
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
        output.block.samples.resize(frameCount);
        rightOperand.resize(frameCount);

        prepareOperand(output.block.samples, left, frameCount, 0, false);
        prepareOperand(rightOperand, right, frameCount, 0, false);
        applyOperation(output.block.samples, rightOperand, operation);
    }

    void processTraversalGrid(
            SignalPayload& output,
            const SignalPayload* left,
            const SignalPayload* right,
            BinarySignalOperation operation,
            const AudioProcessWorkArena* arena) {
        const SignalTraversalGrid* grid = outputGridFor(output.domain, left, right);
        if (grid == nullptr) {
            output.traversalGrid = {};
            return;
        }

        if (bothInputsHaveMismatchedGrids(left, right)) {
            output.traversalGrid = {};
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
            prepareOperand(outputColumn, left, grid->rows, column);
            prepareOperand(rightOperand, right, grid->rows, column);
            applyOperation(outputColumn, rightOperand, operation);
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
            size_t gridColumn = 0,
            bool useTraversalGrid = true) {
        dest.resize(frameCount);
        prepareOperand({ dest.data(), (int) dest.size() }, payload, frameCount, gridColumn, useTraversalGrid);
    }

    static void prepareOperand(
            Buffer<float> dest,
            const SignalPayload* payload,
            size_t frameCount,
            size_t gridColumn = 0,
            bool useTraversalGrid = true) {
        if (payload == nullptr) {
            dest.zero();
            return;
        }

        if (useTraversalGrid && payload->traversalGrid.isValid()) {
            copyTraversalGridColumn(dest, payload->traversalGrid, gridColumn);
            return;
        }

        copyBlockExpandingScalars(dest, payload->block, frameCount);
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
        if (left != nullptr && left->domain == outputDomain && left->traversalGrid.isValid()) {
            return left;
        }

        if (right != nullptr && right->domain == outputDomain && right->traversalGrid.isValid()) {
            return right;
        }

        if (left != nullptr && isConcreteSignalDomain(left->domain) && left->traversalGrid.isValid()) {
            return left;
        }

        if (right != nullptr && isConcreteSignalDomain(right->domain) && right->traversalGrid.isValid()) {
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

    static bool bothInputsHaveMismatchedGrids(
            const SignalPayload* left,
            const SignalPayload* right) {
        if (left == nullptr || right == nullptr
                || !left->traversalGrid.isValid()
                || !right->traversalGrid.isValid()) {
            return false;
        }

        return left->traversalGrid.columns != right->traversalGrid.columns
                || left->traversalGrid.rows != right->traversalGrid.rows
                || !traversalGridMetadataCompatible(
                        left->traversalGrid.metadata,
                        right->traversalGrid.metadata);
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

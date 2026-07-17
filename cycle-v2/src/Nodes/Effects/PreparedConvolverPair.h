#pragma once

#include <Array/Buffer.h>

#include <cstddef>
#include <vector>

namespace CycleV2 {

template<typename Convolver>
class PreparedConvolverPair {
public:
    Convolver& block() { return blockConvolver; }
    Convolver& traversal() { return traversalConvolver; }

    Convolver& beginBlock() {
        activeConvolver = &blockConvolver;
        return *activeConvolver;
    }

    Convolver& beginTraversal() {
        activeConvolver = &traversalConvolver;
        return *activeConvolver;
    }

    void endTraversal() { activeConvolver = &blockConvolver; }

    Convolver* active() { return activeConvolver; }

    bool blockNeedsPreparation(size_t frameCount) const {
        return preparedBlockSize != frameCount;
    }

    bool traversalNeedsPreparation(size_t rowCount) const {
        return preparedTraversalSize != rowCount;
    }

    void markBlockPrepared(size_t frameCount) {
        preparedBlockSize = frameCount;
    }

    void markTraversalPrepared(size_t rowCount) {
        preparedTraversalSize = rowCount;
    }

    void invalidate() {
        preparedBlockSize = 0;
        preparedTraversalSize = 0;
    }

    void prepareScratch(size_t capacity) {
        if (convolutionOutput.size() < capacity) {
            convolutionOutput.resize(capacity);
        }
    }

    Buffer<float> output(size_t frameCount) {
        if (frameCount > convolutionOutput.size()) {
            return {};
        }

        return { convolutionOutput.data(), (int) frameCount };
    }

private:
    Convolver blockConvolver;
    Convolver traversalConvolver;
    Convolver* activeConvolver { &blockConvolver };
    size_t preparedBlockSize {};
    size_t preparedTraversalSize {};
    std::vector<float> convolutionOutput;
};

}

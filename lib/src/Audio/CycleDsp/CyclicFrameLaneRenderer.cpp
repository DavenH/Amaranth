#include "CyclicFrameLaneRenderer.h"

namespace CycleDsp {

namespace {

void rotateFrame(
        Buffer<float> source,
        Buffer<float> destination,
        int phaseSamples) {
    const int unshiftedSamples = source.size() - phaseSamples;
    source.withSize(unshiftedSamples).copyTo(
            destination.section(phaseSamples, unshiftedSamples));
    source.section(unshiftedSamples, phaseSamples).copyTo(
            destination.withSize(phaseSamples));
}

bool validComposition(
        const CyclicFrameCompositionRequest& request,
        CyclicFrameLaneStateView state,
        const CyclicFrameCompositionWorkspace& workspace) {
    const int frameSize = request.currentFrame.size();
    const int halfSize = frameSize / 2;
    return frameSize > 1
            && (frameSize & (frameSize - 1)) == 0
            && request.previousFrame.size() >= frameSize
            && request.fadeIn.size() >= halfSize
            && request.fadeOut.size() >= halfSize
            && state.lastLerpHalf.size() >= halfSize
            && workspace.biasedFrame.size() >= frameSize
            && workspace.shiftedCurrentFrame.size() >= frameSize
            && workspace.shiftedPreviousFrame.size() >= frameSize
            && workspace.previousHalfFrame.size() >= halfSize;
}

}

Buffer<float> CyclicFrameLaneRenderer::compose(
        const CyclicFrameCompositionRequest& request,
        CyclicFrameLaneStateView state,
        const CyclicFrameCompositionWorkspace& workspace) {
    if (!validComposition(request, state, workspace)) {
        return {};
    }

    const int frameSize = request.currentFrame.size();
    const int halfSize = frameSize / 2;
    const int phaseSamples = (int) (frameSize * request.phaseCycles)
            & (frameSize - 1);
    auto biased = workspace.biasedFrame.withSize(frameSize);
    auto lastLerpHalf = state.lastLerpHalf.withSize(halfSize);
    if (request.firstCycle && !request.phaseShiftEnabled) {
        return request.previousFrame.withSize(frameSize);
    }

    biased.zero();
    Buffer<float> current = request.currentFrame.withSize(frameSize);
    Buffer<float> previous = request.previousFrame.withSize(frameSize);
    if (request.phaseShiftEnabled && phaseSamples != 0) {
        if (request.firstCycle) {
            rotateFrame(previous, biased, phaseSamples);
            biased.copyTo(lastLerpHalf);
        } else {
            current = workspace.shiftedCurrentFrame.withSize(frameSize);
            previous = workspace.shiftedPreviousFrame.withSize(frameSize);
            rotateFrame(request.currentFrame, current, phaseSamples);
            rotateFrame(request.previousFrame, previous, phaseSamples);
        }
    }

    if (!request.firstCycle) {
        const float portion = jlimit(0.f, 1.f, request.nextFramePortion);
        if (portion == 0.f) {
            previous.copyTo(biased);
        } else {
            biased.addProduct(previous, 1.f - portion);
            biased.addProduct(current, portion);
        }

        lastLerpHalf.copyTo(workspace.previousHalfFrame.withSize(halfSize));
        biased.copyTo(lastLerpHalf);
        auto incomingHalf = biased.withSize(halfSize);
        incomingHalf.mul(request.fadeIn.withSize(halfSize));
        incomingHalf.addProduct(
                request.fadeOut.withSize(halfSize),
                workspace.previousHalfFrame.withSize(halfSize));
    }
    return biased;
}

}

#include "NodeAudioProcessorSupport.h"

#include "AudioProcessContextUtils.h"

namespace CycleV2 {

void processPassthrough(AudioProcessContext& context) {
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

    copyPayloadBlockExpandingScalars(output, *input, context.frameCount);
    copyTraversalGrid(output, input->traversalGrid);
    publishSingleOutput(context, std::move(output));
}

void processUnaryEffect(
        IUnarySignalOperation& operation,
        UnarySignalProcessor& processor,
        AudioProcessContext& context,
        bool enabled) {
    SignalPayload* input = inputAt(context, 0);
    if (input == nullptr) {
        clearOutput(context);
        return;
    }

    if (!enabled) {
        processPassthrough(context);
        return;
    }

    auto output = makeOutputPayload(context, 0);
    if (context.outputPorts.empty()) {
        output.domain = input->domain;
        output.channelLayout = input->channelLayout;
    }

    processor.process(operation, output, *input, context.frameCount, context.workArena);
    publishSingleOutput(context, std::move(output));
}

}

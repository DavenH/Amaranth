#include "EnvelopeSignalProcessor.h"

namespace CycleV2 {

void EnvelopeSignalProcessor::process(AudioProcessContext& context) const {
    auto output = makeOutputPayload(context, 0);
    const float level = parameterFloat(context.parameters, "level", 1.f);

    payloadBuffer(output, context.frameCount).set(level);
    publishVectorAsTraversalGrid(output, defaultTraversalColumns);
    publishSingleOutput(context, std::move(output));
}

}

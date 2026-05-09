#pragma once

#include <Definitions.h>

namespace Rasterization {
    struct EnvelopeStateValidationContext {
        enum State {
            NormalState,
            Looping,
            Releasing
        };

        State state { NormalState };
        int headIndex {};
        int waveformSize {};
        double waveformEnd {};
        double loopStart {};
        double loopEnd {};
        double loopLength { -1. };
    };

    class EnvelopeStateValidationPolicy {
    public:
        template<typename Params>
        EnvelopeStateValidationContext::State validate(
                Params& params,
                const EnvelopeStateValidationContext& context) const {
            auto state = context.state;

            if (context.waveformSize <= 0) {
                state = EnvelopeStateValidationContext::NormalState;
            }

            clampToWaveform(params, context);

            if (state == EnvelopeStateValidationContext::Looping) {
                if (context.loopLength <= 0.) {
                    state = EnvelopeStateValidationContext::NormalState;
                } else {
                    wrapLoopingParams(params, context);
                }
            } else if (state == EnvelopeStateValidationContext::NormalState
                       && context.loopLength > 0.
                       && params.size() > context.headIndex
                       && isWithin(params[context.headIndex].samplePosition, context.loopStart, context.loopEnd)) {
                state = EnvelopeStateValidationContext::Looping;
            }

            return state;
        }

    private:
        template<typename Params>
        static void clampToWaveform(Params& params, const EnvelopeStateValidationContext& context) {
            for (auto& param : params) {
                if (context.waveformSize <= 0) {
                    param.samplePosition = 0.;
                    param.sampleIndex = 0;
                } else if (param.samplePosition > context.waveformEnd) {
                    param.samplePosition = context.waveformEnd;
                    param.sampleIndex = context.waveformSize - 1;
                }
            }
        }

        template<typename Params>
        static void wrapLoopingParams(Params& params, const EnvelopeStateValidationContext& context) {
            for (int i = context.headIndex; i < (int) params.size(); ++i) {
                double& position = params[i].samplePosition;

                if (position > context.loopEnd) {
                    position = jmax(context.loopStart, position - context.loopLength);
                } else if (position < context.loopStart) {
                    position = jmin(context.loopEnd, position + context.loopLength);
                }
            }
        }

        static bool isWithin(double value, double lower, double upper) {
            return value >= lower && value <= upper;
        }
    };
}

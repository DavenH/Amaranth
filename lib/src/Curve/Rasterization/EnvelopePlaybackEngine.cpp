#include "EnvelopePlaybackEngine.h"

#include <Curve/GuideCurveProvider.h>
#include <Curve/Rasterization/Policies/Envelope/EnvelopePolicies.h>
#include <Curve/Rasterization/Sampling/GuideCurveSampler.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Util/Arithmetic.h>

namespace Rasterization {
    EnvelopePlaybackEngine::EnvelopePlaybackEngine() : outputMemory(8192) {}

    EnvelopePlaybackEngine::EnvelopePlaybackEngine(const EnvelopePlaybackEngine& copy) :
            state       (copy.state)
        ,   outputMemory(8192) {
    }

    EnvelopePlaybackEngine& EnvelopePlaybackEngine::operator =(
            const EnvelopePlaybackEngine& copy) {
        state = copy.state;
        outputMemory.ensureSize(8192);
        renderBuffer = {};
        return *this;
    }

    void EnvelopePlaybackEngine::ensureVoiceCount(int audioVoiceCount) {
        state.ensureVoiceCount(audioVoiceCount + firstAudioVoiceIndex);
    }

    void EnvelopePlaybackEngine::noteOn() {
        state.noteOn(firstAudioVoiceIndex);
    }

    void EnvelopePlaybackEngine::noteOff(const PreparedEnvelopePlaybackView& prepared) {
        state.requestRelease(hasReleaseCurve(prepared));
    }

    void EnvelopePlaybackEngine::resetGraphicVoice() {
        state.voice(graphicVoiceIndex).reset();
    }

    void EnvelopePlaybackEngine::setOneSamplePerCycle(bool enabled) {
        state.oneSamplePerCycle = enabled;
    }

    void EnvelopePlaybackEngine::setMode(EnvelopePlaybackMode mode) {
        state.mode = mode;
    }

    const RenderResult& EnvelopePlaybackEngine::activeResult(
            const PreparedEnvelopePlaybackView& prepared) const {
        return state.mode == EnvelopePlaybackMode::Looping && prepared.loop.sampleable
                ? prepared.loop
                : prepared.display;
    }

    bool EnvelopePlaybackEngine::canLoop(const PreparedEnvelopePlaybackView& prepared) const {
        return prepared.loopIndex >= 0;
    }

    bool EnvelopePlaybackEngine::hasReleaseCurve(const PreparedEnvelopePlaybackView& prepared) const {
        return EnvelopePlaybackPolicy().hasReleaseCurve(
                prepared.display.intercepts,
                prepared.sustainIndex);
    }

    float EnvelopePlaybackEngine::loopLength(const PreparedEnvelopePlaybackView& prepared) const {
        EnvelopePlaybackContext context;
        context.loopIndex = prepared.loopIndex;
        context.sustainIndex = prepared.sustainIndex;
        return EnvelopePlaybackPolicy().loopLength(prepared.display.intercepts, context);
    }

    double EnvelopePlaybackEngine::boundary(const PreparedEnvelopePlaybackView& prepared) const {
        EnvelopePlaybackContext context;
        context.loopIndex = prepared.loopIndex;
        context.sustainIndex = prepared.sustainIndex;
        context.releasing = state.mode == EnvelopePlaybackMode::Releasing;
        return EnvelopePlaybackPolicy().boundary(prepared.display.intercepts, context);
    }

    float EnvelopePlaybackEngine::sampleAt(
            const PreparedEnvelopePlaybackView& prepared,
            double position) const {
        const auto& active = activeResult(prepared);
        return WaveformSampler::sampleAt(active.waveform, !active.sampleable, position);
    }

    float EnvelopePlaybackEngine::sampleAt(
            const PreparedEnvelopePlaybackView& prepared,
            double position,
            int& currentIndex) const {
        const auto& active = activeResult(prepared);
        return WaveformSampler::sampleAt(
                active.waveform,
                !active.sampleable,
                position,
                currentIndex);
    }

    float EnvelopePlaybackEngine::sampleAtDecoupled(
            const PreparedEnvelopePlaybackView& prepared,
            double position,
            GuideCurveContext& context) const {
        const auto& active = activeResult(prepared);
        return GuideCurveSampler::sampleDecoupled(
                active.waveform,
                !active.sampleable,
                position,
                context,
                active.guideCurveRegions,
                prepared.guideCurveProvider,
                prepared.noiseSeed);
    }

    bool EnvelopePlaybackEngine::renderToBuffer(
            const PreparedEnvelopePlaybackView& prepared,
            int numSamples,
            double deltaX,
            int voiceIndex,
            const MeshLibrary::EnvProps& props,
            float tempoScale) {
        if (!props.active) {
            return false;
        }

        jassert(deltaX > 0.);

        EnvelopeRenderTimingContext context;
        context.numSamples = numSamples;
        context.deltaX = deltaX;
        context.tempoScale = tempoScale;
        context.loopLength = loopLength(prepared);
        context.props = &props;
        const auto timing = EnvelopeRenderTimingPolicy().prepare(context);

        if (!state.oneSamplePerCycle) {
            outputMemory.ensureSize(numSamples);
            renderBuffer = outputMemory.withSize(numSamples);
        }

        int bufferPosition = 0;
        bool stillAlive = true;
        while (numSamples - bufferPosition > 0) {
            const int partitionSize = jmin(
                    timing.maxSamplesPerBuffer,
                    numSamples - bufferPosition);
            Buffer<float> partition;
            if (!state.oneSamplePerCycle) {
                partition = Buffer<float>(renderBuffer + bufferPosition, partitionSize);
            }

            int rendered = partitionSize;
            if (stillAlive) {
                rendered = renderPartition(
                        prepared,
                        partition,
                        partitionSize,
                        timing.effectiveDelta,
                        voiceIndex);
                stillAlive = rendered == partitionSize;
            }
            bufferPosition += rendered;

            if (!stillAlive) {
                if (state.oneSamplePerCycle) {
                    break;
                }
                if (bufferPosition < renderBuffer.size()) {
                    renderBuffer.offset(bufferPosition).zero();
                }
            }
        }

        if (props.logarithmic && !state.oneSamplePerCycle) {
            Arithmetic::applyInvLogMapping(renderBuffer, 30);
        }
        return stillAlive;
    }

    int EnvelopePlaybackEngine::renderPartition(
            const PreparedEnvelopePlaybackView& prepared,
            Buffer<float> buffer,
            int numSamples,
            double deltaX,
            int voiceIndex) {
        auto& voice = state.voice(voiceIndex);
        const double end = boundary(prepared);
        const double advancement = numSamples * deltaX;
        bool overextends = voice.samplePosition + advancement > end;

        if (canLoop(prepared)
                && overextends
                && state.mode == EnvelopePlaybackMode::Normal) {
            state.mode = EnvelopePlaybackMode::Looping;
        }
        if (state.consumeReleaseRequest()) {
            beginRelease(prepared);
        }

        int rendered = numSamples;
        switch (state.mode) {
            case EnvelopePlaybackMode::Normal: {
                if (state.oneSamplePerCycle) {
                    voice.sustainLevel = sampleAtDecoupled(
                            prepared,
                            voice.samplePosition,
                            voice.guideCurveContext);
                } else {
                    const int available = jmin(
                            numSamples,
                            int((end - voice.samplePosition) / deltaX));
                    if (available > 0) {
                        WaveformSampler::sampleWithInterval(
                                activeResult(prepared).waveform,
                                buffer.withSize(available),
                                deltaX,
                                voice.samplePosition);
                        voice.sustainLevel = buffer[available - 1];
                    }
                    if (available < numSamples) {
                        buffer.offset(available).set(voice.sustainLevel);
                    }
                }
                voice.samplePosition = jmin(end, voice.samplePosition + advancement);
                break;
            }

            case EnvelopePlaybackMode::Looping: {
                const double length = loopLength(prepared);
                jassert(length > 0.);
                if (state.oneSamplePerCycle) {
                    voice.sustainLevel = sampleAtDecoupled(
                            prepared,
                            voice.samplePosition,
                            voice.guideCurveContext);
                    while (overextends) {
                        voice.samplePosition -= length;
                        overextends = voice.samplePosition + advancement > end;
                    }
                    voice.samplePosition = jmin(end, voice.samplePosition + advancement);
                } else {
                    voice.sampleIndex = activeResult(prepared).waveform.zeroIndex;
                    for (int i = 0; i < numSamples; ++i) {
                        buffer[i] = sampleAt(prepared, voice.samplePosition, voice.sampleIndex);
                        voice.samplePosition += deltaX;
                        if (voice.samplePosition >= end) {
                            voice.samplePosition -= length - deltaX;
                        }
                    }
                    voice.sustainLevel = buffer[numSamples - 1];
                }
                break;
            }

            case EnvelopePlaybackMode::Releasing: {
                if (!hasReleaseCurve(prepared)) {
                    return 0;
                }
                if (state.oneSamplePerCycle) {
                    if (voice.samplePosition <= end) {
                        voice.sustainLevel = state.releaseScale * sampleAtDecoupled(
                                prepared,
                                voice.samplePosition,
                                voice.guideCurveContext);
                    }
                } else {
                    const int available = jmin(
                            numSamples,
                            int((end - voice.samplePosition) / deltaX));
                    Buffer<float> releaseBuffer(buffer, available);
                    WaveformSampler::sampleWithInterval(
                            activeResult(prepared).waveform,
                            releaseBuffer,
                            deltaX,
                            voice.samplePosition);
                    releaseBuffer.mul(state.releaseScale);
                    if (available < numSamples) {
                        buffer.offset(available).zero();
                    }
                    rendered = available;
                }
                voice.samplePosition = jmin(end, voice.samplePosition + advancement);
                break;
            }
        }
        return rendered;
    }

    void EnvelopePlaybackEngine::beginRelease(
            const PreparedEnvelopePlaybackView& prepared) {
        EnvelopeReleaseContext context;
        context.bipolar = prepared.scalingMode == PointScalingMode::Bipolar;
        context.sustainIndex = prepared.sustainIndex;
        const int releaseIndex = EnvelopeReleasePolicy().releaseIndex(context);
        const float releaseLevel = sampleAt(
                prepared,
                prepared.display.intercepts[releaseIndex].x);
        const auto release = EnvelopeReleasePolicy().start(
                prepared.display.intercepts,
                context,
                state.voice(firstAudioVoiceIndex).sustainLevel,
                releaseLevel);
        state.releaseScale = release.scale;
        for (int i = firstAudioVoiceIndex; i < (int) state.allVoices().size(); ++i) {
            state.voice(i).samplePosition = release.position;
        }
    }

    void EnvelopePlaybackEngine::validate(const PreparedEnvelopePlaybackView& prepared) {
        const auto waveform = activeResult(prepared).waveform;
        EnvelopeStateValidationContext context;
        context.state = state.mode == EnvelopePlaybackMode::Looping
                ? EnvelopeStateValidationContext::Looping
                : state.mode == EnvelopePlaybackMode::Releasing
                        ? EnvelopeStateValidationContext::Releasing
                        : EnvelopeStateValidationContext::NormalState;
        context.headIndex = firstAudioVoiceIndex;
        context.waveformSize = waveform.waveX.size();
        context.waveformEnd = waveform.waveX.empty() ? 0. : waveform.waveX.back();
        context.loopLength = loopLength(prepared);
        context.loopStart = prepared.loopIndex >= 0
                && prepared.loopIndex < (int) prepared.display.intercepts.size()
                        ? prepared.display.intercepts[prepared.loopIndex].x
                        : 0.;
        context.loopEnd = prepared.sustainIndex >= 0
                && prepared.sustainIndex < (int) prepared.display.intercepts.size()
                        ? prepared.display.intercepts[prepared.sustainIndex].x
                        : 0.;
        const auto validated = EnvelopeStateValidationPolicy().validate(state.allVoices(), context);
        state.mode = validated == EnvelopeStateValidationContext::Looping
                ? EnvelopePlaybackMode::Looping
                : validated == EnvelopeStateValidationContext::Releasing
                        ? EnvelopePlaybackMode::Releasing
                        : EnvelopePlaybackMode::Normal;
    }

    void EnvelopePlaybackEngine::simulateStart(double& position) {
        noteOn();
        position = 0.;
    }

    bool EnvelopePlaybackEngine::simulateStop(
            const PreparedEnvelopePlaybackView& prepared,
            double& position) {
        noteOff(prepared);
        if (!hasReleaseCurve(prepared)) {
            return false;
        }
        position = prepared.display.intercepts[prepared.sustainIndex].x;
        return true;
    }

    bool EnvelopePlaybackEngine::simulateRender(
            const PreparedEnvelopePlaybackView& prepared,
            double advancement,
            double& position,
            const MeshLibrary::EnvProps& props,
            float tempoScale) {
        if (prepared.display.intercepts.empty()
                || (state.mode != EnvelopePlaybackMode::Releasing
                    && !isPositiveAndBelow(
                            prepared.sustainIndex,
                            (int) prepared.display.intercepts.size()))) {
            return false;
        }

        EnvelopeRenderTimingContext timingContext;
        timingContext.deltaX = advancement;
        timingContext.tempoScale = tempoScale;
        timingContext.loopLength = loopLength(prepared);
        timingContext.props = &props;
        advancement = EnvelopeRenderTimingPolicy().prepare(timingContext).effectiveDelta;

        const double end = boundary(prepared);
        bool overextends = position + advancement > end;
        if (canLoop(prepared)
                && overextends
                && state.mode == EnvelopePlaybackMode::Normal) {
            state.mode = EnvelopePlaybackMode::Looping;
        }
        if (state.consumeReleaseRequest()) {
            EnvelopeReleaseContext releaseContext;
            releaseContext.bipolar = prepared.scalingMode == PointScalingMode::Bipolar;
            releaseContext.sustainIndex = prepared.sustainIndex;
            const int releaseIndex = EnvelopeReleasePolicy().releaseIndex(releaseContext);
            position = prepared.display.intercepts[releaseIndex].x;
        }

        switch (state.mode) {
            case EnvelopePlaybackMode::Normal:
                position = jmin(end, position + advancement);
                break;

            case EnvelopePlaybackMode::Looping: {
                const double length = loopLength(prepared);
                if (state.oneSamplePerCycle) {
                    while (overextends) {
                        position -= length;
                        overextends = position + advancement > end;
                    }
                    position = jmin(end, position + advancement);
                } else {
                    position += advancement;
                    while (position >= end) {
                        position -= length;
                    }
                }
                break;
            }

            case EnvelopePlaybackMode::Releasing:
                if (!hasReleaseCurve(prepared)) {
                    return false;
                }
                position = jmin(end, position + advancement);
                if (position == end) {
                    return false;
                }
                break;
        }
        return true;
    }

    void EnvelopePlaybackEngine::randomizeVoiceOffsets(int tableSize) {
        Random random(Time::currentTimeMillis());
        for (auto& voice : state.allVoices()) {
            voice.guideCurveContext.phaseOffsetSeed = random.nextInt(tableSize);
            voice.guideCurveContext.vertOffsetSeed = random.nextInt(tableSize);
        }
    }
}

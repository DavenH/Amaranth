#pragma once

#include <utility>

#include <App/MeshLibrary.h>
#include <Array/ScopedAlloc.h>
#include <Curve/Rasterization/EnvelopePlaybackState.h>
#include <Curve/Rasterization/GuideCurveOffsetSeeds.h>
#include <Curve/Rasterization/Policies/Core/PointScalingPolicy.h>
#include <Curve/Rasterization/RenderResult.h>

class GuideCurveProvider;

namespace Rasterization {
    struct PreparedEnvelopePlaybackView {
        const RenderResult& display;
        const RenderResult& loop;
        int loopIndex { -1 };
        int sustainIndex { -1 };
        PointScalingMode scalingMode { PointScalingMode::Unipolar };
        GuideCurveProvider* guideCurveProvider {};
        int noiseSeed { -1 };
    };

    class EnvelopePlaybackEngine {
    public:
        enum { graphicVoiceIndex = 0, firstAudioVoiceIndex = 1 };

        EnvelopePlaybackEngine();
        EnvelopePlaybackEngine(const EnvelopePlaybackEngine& copy);
        EnvelopePlaybackEngine& operator =(const EnvelopePlaybackEngine& copy);

        void ensureVoiceCount(int audioVoiceCount);
        void noteOn();
        void noteOff(const PreparedEnvelopePlaybackView& prepared);
        void resetGraphicVoice();
        void setOneSamplePerCycle(bool enabled);
        void setMode(EnvelopePlaybackMode mode);
        void validate(const PreparedEnvelopePlaybackView& prepared);

        bool renderToBuffer(
                const PreparedEnvelopePlaybackView& prepared,
                int numSamples,
                double deltaX,
                int voiceIndex,
                const MeshLibrary::EnvProps& props,
                float tempoScale);
        void simulateStart(double& position);
        bool simulateStop(const PreparedEnvelopePlaybackView& prepared, double& position);
        bool simulateRender(
                const PreparedEnvelopePlaybackView& prepared,
                double advancement,
                double& position,
                const MeshLibrary::EnvProps& props,
                float tempoScale);

        void deriveVoiceOffsets(int tableSize, GuideCurveSeed seed);

        std::pair<int, int> voiceOffsetSeeds(int voiceIndex) const {
            const auto& context = state.voice(voiceIndex).guideCurveContext;
            return { context.phaseOffsetSeed, context.vertOffsetSeed };
        }

        EnvelopePlaybackMode mode() const { return state.mode; }
        bool oneSamplePerCycle() const { return state.oneSamplePerCycle; }
        float sustainLevel(int voiceIndex) const { return state.voice(voiceIndex).sustainLevel; }
        Buffer<float> output() { return renderBuffer; }

    private:
        const RenderResult& activeResult(const PreparedEnvelopePlaybackView& prepared) const;
        bool canLoop(const PreparedEnvelopePlaybackView& prepared) const;
        bool hasReleaseCurve(const PreparedEnvelopePlaybackView& prepared) const;
        float loopLength(const PreparedEnvelopePlaybackView& prepared) const;
        double boundary(const PreparedEnvelopePlaybackView& prepared) const;

        float sampleAt(const PreparedEnvelopePlaybackView& prepared, double position) const;
        float sampleAt(
                const PreparedEnvelopePlaybackView& prepared,
                double position,
                int& currentIndex) const;
        float sampleAtDecoupled(
                const PreparedEnvelopePlaybackView& prepared,
                double position,
                GuideCurveContext& context) const;
        int renderPartition(
                const PreparedEnvelopePlaybackView& prepared,
                Buffer<float> buffer,
                int numSamples,
                double deltaX,
                int voiceIndex);
        void beginRelease(const PreparedEnvelopePlaybackView& prepared);

        EnvelopePlaybackState state;
        ScopedAlloc<float> outputMemory;
        Buffer<float> renderBuffer;
    };
}

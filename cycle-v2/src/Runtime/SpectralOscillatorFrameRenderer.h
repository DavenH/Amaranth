#pragma once

#include "Graph/GraphCompiler.h"
#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Runtime/PreparedOscillatorRegion.h"
#include "Runtime/PreparedCycleEnvelopeBank.h"
#include "Runtime/PreparedTrimeshMorphBinding.h"
#include "Runtime/TrimeshMorphResolver.h"

#include <Algo/FFT.h>
#include <Array/ScopedAlloc.h>
#include <Curve/Rasterization/Rasterizer/VoiceRasterizer.h>

#include <array>
#include <memory>
#include <vector>

namespace CycleV2 {

class SpectralOscillatorFrameRenderer {
public:
    static bool supports(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region);

    bool prepare(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region,
            int maximumFrameSize,
            const std::vector<NodeAudioProcessor*>& processors = {},
            int laneCount = 1,
            const String& pitchEnvelopeNodeId = {});
    void reset();
    void applyLifecycleEvent(const NoteLifecycleEvent& event);
    bool renderFrame(
            int frameSize,
            int midiNote,
            Buffer<float> left,
            Buffer<float> right);
    bool renderFrame(
            int frameSize,
            int midiNote,
            const PreparedOscillatorProcessContext& context,
            size_t blockSampleOffset,
            double voiceSamplePosition,
            size_t elapsedSamples,
            Buffer<float> left,
            Buffer<float> right);
    size_t frameRenderCount() const { return renderCount; }
    bool hasPitchEnvelope() const { return cycleEnvelopes.hasPitchEnvelope(); }
    float pitchEnvelopeValue(int laneIndex) const {
        return cycleEnvelopes.pitchValue(laneIndex);
    }

private:
    enum class OperationType {
        TimeTrimesh,
        SpectralTrimesh,
        SpectralLayer,
        Fft,
        Ifft,
        Add,
        Multiply
    };

    struct Operation {
        OperationType type { OperationType::TimeTrimesh };
        PortDomain outputDomain { PortDomain::TimeSignal };
        int leftInput { -1 };
        int rightInput { -1 };
        std::array<int, 2> outputs { -1, -1 };
        PreparedTrimeshMorphBinding morphBinding;
        std::shared_ptr<const TrimeshConfiguration> configuration;
        float pan { 0.5f };
        bool multiplicative {};
        std::unique_ptr<Rasterization::VoiceRasterizer> timeRasterizer;
        std::unique_ptr<Rasterization::VoiceCycleState> timeState;
        std::unique_ptr<TrimeshBlockwiseDsp> spectralRasterizer;
        TrimeshMorphResolver morphResolver;
    };

    bool renderFrameInternal(
            int frameSize,
            int midiNote,
            const PreparedOscillatorProcessContext* context,
            size_t blockSampleOffset,
            double voiceSamplePosition,
            size_t elapsedSamples,
            Buffer<float> left,
            Buffer<float> right);
    static int valueCount(PortDomain domain, int frameSize);
    Buffer<float> slot(int slotIndex, int channel, int valueCount);
    Transform* transformFor(int frameSize);
    void prepareFrameRandom(const PreparedOscillatorProcessContext* context);

    int maximumFrameSize {};
    int slotStride {};
    int outputSlot { -1 };
    bool hasSpectralMesh {};
    size_t renderCount {};
    std::vector<Operation> operations;
    PreparedCycleEnvelopeBank cycleEnvelopes;
    std::vector<std::unique_ptr<Transform>> transforms;
    ScopedAlloc<float> slotMemory;
    ScopedAlloc<float> magnitudeScratch;
    ScopedAlloc<float> phaseScratch;
    ScopedAlloc<float> phaseHarmonicScale;
    Random frameRandom;
    int64_t frameRandomSeed {};
    bool lifecycleSeedReady {};
};

}

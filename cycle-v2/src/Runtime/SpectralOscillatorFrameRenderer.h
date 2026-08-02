#pragma once

#include "../Graph/GraphCompiler.h"
#include "../Nodes/Trimesh/TrimeshBlockwiseDsp.h"

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
            int maximumFrameSize);
    void reset();
    bool renderFrame(
            int frameSize,
            Buffer<float> left,
            Buffer<float> right);

private:
    enum class OperationType {
        TimeTrimesh,
        SpectralTrimesh,
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
        std::shared_ptr<const TrimeshConfiguration> configuration;
        std::unique_ptr<Rasterization::VoiceRasterizer> timeRasterizer;
        std::unique_ptr<Rasterization::VoiceCycleState> timeState;
        std::unique_ptr<TrimeshBlockwiseDsp> spectralRasterizer;
    };

    static int valueCount(PortDomain domain, int frameSize);
    Buffer<float> slot(int slotIndex, int valueCount);
    Transform* transformFor(int frameSize);

    int maximumFrameSize {};
    int slotStride {};
    int outputSlot { -1 };
    std::vector<Operation> operations;
    std::vector<std::unique_ptr<Transform>> transforms;
    ScopedAlloc<float> slotMemory;
};

}

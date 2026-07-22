#pragma once

#include <array>

#include "../../Runtime/NodeAudioProcessor.h"
#include "../../Runtime/NodePreviewProcessor.h"

namespace CycleV2 {

enum class ModulationSourceMode {
    VoiceTime,
    Velocity,
    InverseVelocity,
    KeyScale,
    ModWheel,
    ChannelPressure,
    MidiController,
    Constant
};

struct PreviewControlContext {
    float voiceTime {};
    float velocity { 1.f };
    int noteNumber { 60 };
    int lowestNote { 0 };
    int highestNote { 127 };
    float channelPressure {};
    std::array<float, 128> controllers {};
};

struct ModulationSourceConfiguration final : public INodeDspConfiguration {
    ModulationSourceMode mode { ModulationSourceMode::ModWheel };
    int controller { 1 };
    float constant { 0.5f };

    AudioModuleRole role() const override { return AudioModuleRole::ModulationSource; }
};

class ModulationSource {
public:
    static ModulationSourceMode modeFromId(const String& id);
    static String idForMode(ModulationSourceMode mode);
    static float normalizeKey(int note, int lowestNote, int highestNote);
    static float evaluate(
            const ModulationSourceConfiguration& configuration,
            const PreviewControlContext& context);
    static std::shared_ptr<const ModulationSourceConfiguration> buildConfiguration(
            const std::vector<NodeParameter>& parameters);
};

std::unique_ptr<NodeAudioProcessor> createModulationSourceAudioProcessor();
std::unique_ptr<NodePreviewProcessor> createModulationSourcePreviewProcessor();

}

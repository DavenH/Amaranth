#pragma once

#include "../Graph/NodeGraph.h"

namespace CycleV2 {

enum class AudioModuleRole {
    None,
    VoiceContext,
    WaveSource,
    ImageSource,
    MeshSource,
    Fft,
    Ifft,
    Add,
    Multiply,
    Envelope,
    GuideCurve,
    ImpulseResponse,
    Waveshaper,
    Reverb,
    Delay,
    Spy,
    StereoSplit,
    StereoJoin,
    Output,
    GenericProcessor
};

enum class PreviewModuleRole {
    None,
    VoiceContext,
    Waveform,
    Image,
    MeshSurface,
    SpectrumMagnitude,
    SpectrumPhase,
    Envelope,
    ImpulseResponse,
    Waveshaper,
    SignalSpy,
    OutputMeters,
    Generic
};

struct NodeModuleDescriptor {
    NodeKind kind { NodeKind::GenericProcessor };
    AudioModuleRole audioRole { AudioModuleRole::None };
    PreviewModuleRole previewRole { PreviewModuleRole::None };
    bool executable {};
    bool previewable {};
    bool cycle1AdapterBacked {};
    String cycle1Reference;
};

class NodeModuleRegistry {
public:
    NodeModuleDescriptor descriptorFor(NodeKind kind) const;
};

String labelForAudioModuleRole(AudioModuleRole role);
String labelForPreviewModuleRole(PreviewModuleRole role);

}

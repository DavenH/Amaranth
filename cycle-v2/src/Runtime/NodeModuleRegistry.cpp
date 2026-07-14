#include "NodeModuleRegistry.h"

#include <utility>

namespace CycleV2 {

namespace {

NodeModuleDescriptor descriptor(
        NodeKind kind,
        AudioModuleRole audioRole,
        PreviewModuleRole previewRole,
        bool executable,
        bool previewable,
        String cycle1Reference = {}) {
    return {
            kind,
            audioRole,
            previewRole,
            executable,
            previewable,
            cycle1Reference.isNotEmpty(),
            std::move(cycle1Reference)
    };
}

}

NodeModuleDescriptor NodeModuleRegistry::descriptorFor(NodeKind kind) const {
    const auto* definition = NodeDefinitionRegistry::instance().find(kind);
    if (definition == nullptr) {
        definition = NodeDefinitionRegistry::instance().find(NodeKind::GenericProcessor);
    }
    jassert(definition != nullptr);
    return descriptor(
            definition->kind,
            definition->audioRole,
            definition->previewRole,
            definition->executable,
            definition->previewable,
            definition->cycle1Reference);
}

String labelForAudioModuleRole(AudioModuleRole role) {
    switch (role) {
        case AudioModuleRole::None:              return "None";
        case AudioModuleRole::VoiceContext:      return "Voice Context";
        case AudioModuleRole::WaveSource:        return "Wave Source";
        case AudioModuleRole::ImageSource:       return "Image Source";
        case AudioModuleRole::MeshSource:        return "Mesh Source";
        case AudioModuleRole::Fft:               return "FFT";
        case AudioModuleRole::Ifft:              return "IFFT";
        case AudioModuleRole::Add:               return "Add";
        case AudioModuleRole::Multiply:          return "Multiply";
        case AudioModuleRole::Envelope:          return "Envelope";
        case AudioModuleRole::GuideCurve:        return "Guide Curve";
        case AudioModuleRole::ImpulseResponse:   return "Impulse Response";
        case AudioModuleRole::Waveshaper:        return "Waveshaper";
        case AudioModuleRole::Reverb:            return "Reverb";
        case AudioModuleRole::Delay:             return "Delay";
        case AudioModuleRole::Spy:               return "Spy";
        case AudioModuleRole::StereoSplit:       return "Stereo Split";
        case AudioModuleRole::StereoJoin:        return "Stereo Join";
        case AudioModuleRole::Output:            return "Output";
        case AudioModuleRole::GenericProcessor:  return "Generic Processor";
        default:                                 return "Unknown";
    }
}

String labelForPreviewModuleRole(PreviewModuleRole role) {
    switch (role) {
        case PreviewModuleRole::None:              return "None";
        case PreviewModuleRole::VoiceContext:      return "Voice Context";
        case PreviewModuleRole::Waveform:          return "Waveform";
        case PreviewModuleRole::Image:             return "Image";
        case PreviewModuleRole::MeshSurface:       return "Mesh Surface";
        case PreviewModuleRole::SpectrumMagnitude: return "Spectrum Magnitude";
        case PreviewModuleRole::SpectrumPhase:     return "Spectrum Phase";
        case PreviewModuleRole::Envelope:          return "Envelope";
        case PreviewModuleRole::ImpulseResponse:   return "Impulse Response";
        case PreviewModuleRole::Waveshaper:        return "Waveshaper";
        case PreviewModuleRole::SignalSpy:         return "Signal Spy";
        case PreviewModuleRole::OutputMeters:      return "Output Meters";
        case PreviewModuleRole::Generic:           return "Generic";
        default:                                   return "Unknown";
    }
}

}

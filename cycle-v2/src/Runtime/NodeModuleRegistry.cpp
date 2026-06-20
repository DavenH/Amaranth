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
    switch (kind) {
        case NodeKind::VoiceContext:
            return descriptor(kind, AudioModuleRole::VoiceContext, PreviewModuleRole::VoiceContext, true, false);

        case NodeKind::WaveSource:
            return descriptor(kind, AudioModuleRole::WaveSource, PreviewModuleRole::Waveform, true, true);

        case NodeKind::ImageSource:
            return descriptor(kind, AudioModuleRole::ImageSource, PreviewModuleRole::Image, true, true);

        case NodeKind::TrilinearMesh:
            return descriptor(
                    kind,
                    AudioModuleRole::MeshSource,
                    PreviewModuleRole::MeshSurface,
                    true,
                    true,
                    "cycle/src/Curve/Rasterization/Rasterizer/VoiceMeshRasterizer.cpp");

        case NodeKind::Fft:
            return descriptor(kind, AudioModuleRole::Fft, PreviewModuleRole::None, true, false);

        case NodeKind::Ifft:
            return descriptor(kind, AudioModuleRole::Ifft, PreviewModuleRole::None, true, false);

        case NodeKind::Envelope:
            return descriptor(
                    kind,
                    AudioModuleRole::Envelope,
                    PreviewModuleRole::Envelope,
                    true,
                    true,
                    "cycle/src/Inter/EnvelopeInter2D.cpp");

        case NodeKind::Add:
            return descriptor(kind, AudioModuleRole::Add, PreviewModuleRole::None, true, false);

        case NodeKind::Multiply:
            return descriptor(kind, AudioModuleRole::Multiply, PreviewModuleRole::None, true, false);

        case NodeKind::GuideCurve:
            return descriptor(
                    kind,
                    AudioModuleRole::GuideCurve,
                    PreviewModuleRole::Envelope,
                    true,
                    true,
                    "cycle/src/UI/VertexPanels/GuideCurvePanel.cpp");

        case NodeKind::ImpulseResponse:
            return descriptor(
                    kind,
                    AudioModuleRole::ImpulseResponse,
                    PreviewModuleRole::ImpulseResponse,
                    true,
                    true,
                    "cycle/src/Audio/Effects/IrModeller.cpp");

        case NodeKind::Waveshaper:
            return descriptor(
                    kind,
                    AudioModuleRole::Waveshaper,
                    PreviewModuleRole::Waveshaper,
                    true,
                    true,
                    "cycle/src/Audio/Effects/WaveShaper.cpp");

        case NodeKind::Reverb:
            return descriptor(
                    kind,
                    AudioModuleRole::Reverb,
                    PreviewModuleRole::None,
                    true,
                    false,
                    "cycle/src/Audio/Effects/Reverb.cpp");

        case NodeKind::Delay:
            return descriptor(
                    kind,
                    AudioModuleRole::Delay,
                    PreviewModuleRole::None,
                    true,
                    false,
                    "cycle/src/Audio/Effects/Delay.cpp");

        case NodeKind::Spy:
            return descriptor(kind, AudioModuleRole::Spy, PreviewModuleRole::SignalSpy, true, true);

        case NodeKind::StereoSplit:
            return descriptor(kind, AudioModuleRole::StereoSplit, PreviewModuleRole::None, true, false);

        case NodeKind::StereoJoin:
            return descriptor(kind, AudioModuleRole::StereoJoin, PreviewModuleRole::None, true, false);

        case NodeKind::Output:
            return descriptor(kind, AudioModuleRole::Output, PreviewModuleRole::None, true, false);

        case NodeKind::GenericProcessor:
        default:
            return descriptor(kind, AudioModuleRole::GenericProcessor, PreviewModuleRole::Generic, true, true);
    }
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

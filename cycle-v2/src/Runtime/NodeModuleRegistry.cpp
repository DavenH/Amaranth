#include "NodeModuleRegistry.h"

namespace CycleV2 {

NodeModuleDescriptor NodeModuleRegistry::descriptorFor(NodeKind kind) const {
    switch (kind) {
        case NodeKind::VoiceContext:
            return { kind, AudioModuleRole::VoiceContext, PreviewModuleRole::VoiceContext, true, false, false };

        case NodeKind::WaveSource:
            return { kind, AudioModuleRole::WaveSource, PreviewModuleRole::Waveform, true, true, false };

        case NodeKind::ImageSource:
            return { kind, AudioModuleRole::ImageSource, PreviewModuleRole::Image, true, true, false };

        case NodeKind::TrilinearWaveSurface:
        case NodeKind::TrilinearMesh:
            return { kind, AudioModuleRole::MeshSource, PreviewModuleRole::MeshSurface, true, true, true };

        case NodeKind::Fft:
            return { kind, AudioModuleRole::Fft, PreviewModuleRole::None, true, false, false };

        case NodeKind::SpectralMagnitudeProcessor:
            return { kind, AudioModuleRole::MeshSource, PreviewModuleRole::SpectrumMagnitude, true, true, true };

        case NodeKind::SpectralPhaseProcessor:
            return { kind, AudioModuleRole::MeshSource, PreviewModuleRole::SpectrumPhase, true, true, true };

        case NodeKind::Ifft:
            return { kind, AudioModuleRole::Ifft, PreviewModuleRole::None, true, false, false };

        case NodeKind::Envelope:
            return { kind, AudioModuleRole::Envelope, PreviewModuleRole::Envelope, true, true, true };

        case NodeKind::Add:
            return { kind, AudioModuleRole::Add, PreviewModuleRole::None, true, false, false };

        case NodeKind::Multiply:
            return { kind, AudioModuleRole::Multiply, PreviewModuleRole::None, true, false, false };

        case NodeKind::GuideCurve:
            return { kind, AudioModuleRole::GuideCurve, PreviewModuleRole::Envelope, true, true, true };

        case NodeKind::ImpulseResponse:
            return { kind, AudioModuleRole::ImpulseResponse, PreviewModuleRole::ImpulseResponse, true, true, true };

        case NodeKind::Waveshaper:
            return { kind, AudioModuleRole::Waveshaper, PreviewModuleRole::Waveshaper, true, true, true };

        case NodeKind::Reverb:
            return { kind, AudioModuleRole::Reverb, PreviewModuleRole::None, true, false, true };

        case NodeKind::Delay:
            return { kind, AudioModuleRole::Delay, PreviewModuleRole::None, true, false, true };

        case NodeKind::StereoSplit:
            return { kind, AudioModuleRole::StereoSplit, PreviewModuleRole::None, true, false, false };

        case NodeKind::StereoJoin:
            return { kind, AudioModuleRole::StereoJoin, PreviewModuleRole::None, true, false, false };

        case NodeKind::Output:
            return { kind, AudioModuleRole::Output, PreviewModuleRole::OutputMeters, true, true, false };

        case NodeKind::GenericProcessor:
        default:
            return { kind, AudioModuleRole::GenericProcessor, PreviewModuleRole::Generic, true, true, false };
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
        case PreviewModuleRole::OutputMeters:      return "Output Meters";
        case PreviewModuleRole::Generic:           return "Generic";
        default:                                   return "Unknown";
    }
}

}

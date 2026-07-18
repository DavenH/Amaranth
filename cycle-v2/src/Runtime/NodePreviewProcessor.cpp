#include "NodePreviewProcessor.h"

#include "PreviewProcessorFactories.h"

namespace CycleV2 {

std::unique_ptr<NodePreviewProcessor> NodePreviewProcessorFactory::create(
        PreviewModuleRole role) const {
    using Factory = std::unique_ptr<NodePreviewProcessor> (*)();

    struct Registration {
        PreviewModuleRole role;
        Factory factory;
    };

    static const Registration registrations[] {
            { PreviewModuleRole::Waveform, createWaveformPreviewProcessor },
            { PreviewModuleRole::Image, createImagePreviewProcessor },
            { PreviewModuleRole::MeshSurface, createTrimeshPreviewProcessor },
            { PreviewModuleRole::SpectrumMagnitude, createSpectrumMagnitudePreviewProcessor },
            { PreviewModuleRole::SpectrumPhase, createSpectrumPhasePreviewProcessor },
            { PreviewModuleRole::Envelope, createEnvelopePreviewProcessor },
            { PreviewModuleRole::ImpulseResponse, createImpulseResponsePreviewProcessor },
            { PreviewModuleRole::Waveshaper, createWaveshaperPreviewProcessor },
            { PreviewModuleRole::EqualizerResponse, createEqualizerPreviewProcessor },
            { PreviewModuleRole::OutputMeters, createOutputMetersPreviewProcessor },
            { PreviewModuleRole::SignalSpy, createSignalSpyPreviewProcessor },
            { PreviewModuleRole::VoiceContext, createVoiceContextPreviewProcessor },
            { PreviewModuleRole::Generic, createGenericPreviewProcessor }
    };

    for (const auto& registration : registrations) {
        if (registration.role == role) {
            return registration.factory();
        }
    }

    return {};
}

}

#include "NodeAudioProcessor.h"

#include "AudioProcessorFactories.h"

namespace CycleV2 {

void NodeAudioProcessor::prepareExecution(const AudioExecutionSpec&) {
}

void NodeAudioProcessor::adoptConfiguration(const PublishedNodeConfiguration&) {
}

bool NodeAudioProcessor::serviceNonRealtimePreparation() {
    return false;
}

std::unique_ptr<NodeAudioProcessor> NodeAudioProcessorFactory::create(AudioModuleRole role) const {
    using Factory = std::unique_ptr<NodeAudioProcessor> (*)();

    struct Registration {
        AudioModuleRole role;
        Factory factory;
    };

    static const Registration registrations[] {
            { AudioModuleRole::WaveSource, createWaveSourceAudioProcessor },
            { AudioModuleRole::ImageSource, createImageSourceAudioProcessor },
            { AudioModuleRole::MeshSource, createTrimeshAudioProcessor },
            { AudioModuleRole::Add, createAddAudioProcessor },
            { AudioModuleRole::Multiply, createMultiplyAudioProcessor },
            { AudioModuleRole::StereoJoin, createStereoJoinAudioProcessor },
            { AudioModuleRole::StereoSplit, createStereoSplitAudioProcessor },
            { AudioModuleRole::Output, createOutputAudioProcessor },
            { AudioModuleRole::GenericProcessor, createGenericAudioProcessor },
            { AudioModuleRole::VoiceContext, createVoiceContextAudioProcessor },
            { AudioModuleRole::GuideCurve, createGuideCurveAudioProcessor },
            { AudioModuleRole::Envelope, createEnvelopeAudioProcessor },
            { AudioModuleRole::Fft, createFftAudioProcessor },
            { AudioModuleRole::Ifft, createIfftAudioProcessor },
            { AudioModuleRole::ImpulseResponse, createImpulseResponseAudioProcessor },
            { AudioModuleRole::Waveshaper, createWaveshaperAudioProcessor },
            { AudioModuleRole::Reverb, createReverbAudioProcessor },
            { AudioModuleRole::Delay, createDelayAudioProcessor },
            { AudioModuleRole::Equalizer, createEqualizerAudioProcessor }
    };

    for (const auto& registration : registrations) {
        if (registration.role == role) {
            return registration.factory();
        }
    }

    return {};
}

}

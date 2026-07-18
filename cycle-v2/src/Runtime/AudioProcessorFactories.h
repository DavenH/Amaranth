#pragma once

#include <memory>

#include "NodeAudioProcessor.h"

namespace CycleV2 {

std::unique_ptr<NodeAudioProcessor> createWaveSourceAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createImageSourceAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createTrimeshAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createAddAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createMultiplyAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createStereoJoinAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createStereoSplitAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createOutputAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createGenericAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createVoiceContextAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createGuideCurveAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createEnvelopeAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createFftAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createIfftAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createImpulseResponseAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createWaveshaperAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createReverbAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createDelayAudioProcessor();
std::unique_ptr<NodeAudioProcessor> createEqualizerAudioProcessor();

}

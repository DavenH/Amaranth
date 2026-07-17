#pragma once

#include <memory>

#include "NodePreviewProcessor.h"

namespace CycleV2 {

std::unique_ptr<NodePreviewProcessor> createWaveformPreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createImagePreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createTrimeshPreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createSpectrumMagnitudePreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createSpectrumPhasePreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createEnvelopePreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createImpulseResponsePreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createWaveshaperPreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createOutputMetersPreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createSignalSpyPreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createVoiceContextPreviewProcessor();
std::unique_ptr<NodePreviewProcessor> createGenericPreviewProcessor();

}

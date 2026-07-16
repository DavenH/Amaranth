#pragma once

#include <vector>

#include <Curve/Rasterization/Sampling/GuideCurveSampler.h>

namespace Rasterization {
    enum class EnvelopePlaybackMode {
        Normal,
        Looping,
        Releasing
    };

    struct EnvelopeVoicePlaybackState {
        void reset() {
            samplePosition = 0.;
            sampleIndex = 0;
            guideCurveContext.currentIndex = 0;
        }

        int sampleIndex {};
        float sustainLevel { 1.f };
        double samplePosition {};
        GuideCurveContext guideCurveContext;
    };

    class EnvelopePlaybackState {
    public:
        explicit EnvelopePlaybackState(int initialVoiceCount = 2) : voices((size_t) initialVoiceCount) {}

        void ensureVoiceCount(int voiceCount) {
            while ((int) voices.size() < voiceCount) {
                voices.emplace_back();
            }
        }

        void noteOn(int firstAudioVoice) {
            mode = EnvelopePlaybackMode::Normal;
            releasePending = false;
            for (int i = firstAudioVoice; i < (int) voices.size(); ++i) {
                voices[(size_t) i].reset();
            }
        }

        void requestRelease(bool hasReleaseCurve) {
            if (mode != EnvelopePlaybackMode::Releasing && hasReleaseCurve) {
                mode = EnvelopePlaybackMode::Releasing;
                releasePending = true;
            }
        }

        bool consumeReleaseRequest() {
            const bool result = releasePending;
            releasePending = false;
            return result;
        }

        EnvelopeVoicePlaybackState& voice(int index) { return voices[(size_t) index]; }
        const EnvelopeVoicePlaybackState& voice(int index) const { return voices[(size_t) index]; }

        std::vector<EnvelopeVoicePlaybackState>& allVoices() { return voices; }
        const std::vector<EnvelopeVoicePlaybackState>& allVoices() const { return voices; }

        EnvelopePlaybackMode mode { EnvelopePlaybackMode::Normal };
        float releaseScale { 1.f };
        bool oneSamplePerCycle {};

    private:
        bool releasePending {};
        std::vector<EnvelopeVoicePlaybackState> voices;
    };
}

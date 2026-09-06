#include <catch2/catch_test_macros.hpp>

#include <Audio/AudioSourceProcessor.h>

namespace {
    class RecordingAudioSourceProcessor : public AudioSourceProcessor {
    public:
        void prepareToPlay(int, double) override {}
        void releaseResources() override {}

        void processBlock(AudioSampleBuffer& buffer, MidiBuffer&) override {
            channelCount = buffer.getNumChannels();
            sampleCount = buffer.getNumSamples();
            buffer.clear();
            for (int channel = 0; channel < channelCount; ++channel) {
                buffer.addSample(channel, 0, (float) channel + 1.0f);
            }
        }

        int channelCount {};
        int sampleCount {};
    };
}

TEST_CASE("Audio source processor preserves device channels and block offset",
        "[audio][device][bridge]") {
    RecordingAudioSourceProcessor processor;
    AudioSampleBuffer deviceBuffer(2, 32);
    deviceBuffer.clear();
    AudioSourceChannelInfo channelInfo(&deviceBuffer, 5, 12);

    processor.getNextAudioBlock(channelInfo);

    REQUIRE(processor.channelCount == 2);
    REQUIRE(processor.sampleCount == 12);
    REQUIRE(deviceBuffer.getSample(0, 4) == 0.0f);
    REQUIRE(deviceBuffer.getSample(0, 5) == 1.0f);
    REQUIRE(deviceBuffer.getSample(1, 5) == 2.0f);
    REQUIRE(deviceBuffer.getSample(0, 17) == 0.0f);
}

#pragma once

class RealTimePitchTraceListener {
public:
    struct Frame {
        explicit Frame(
            int detectedMidiIn
        )
            : detectedMidi(detectedMidiIn)
             {}

        Frame() = default;

        int detectedMidi = -1;
    };

    virtual ~RealTimePitchTraceListener() = default;
    virtual void onPitchFrame(int bestKeyIndex,
        const Buffer<float>& windowedAudio,
        const Buffer<float>& kernelCorrelations,
        const Buffer<float>& fftMagnitudes
        ) {}
};

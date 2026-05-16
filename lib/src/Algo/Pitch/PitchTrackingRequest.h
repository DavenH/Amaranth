#pragma once

struct PitchTrackingRequest {
    float minFrequencyHz = 20.f;
    float maxFrequencyHz = 3000.f;
    double frequencyOfA4 = 440.0;

    int firstMidiNote = 21;
    int numMidiNotes  = 88;
};

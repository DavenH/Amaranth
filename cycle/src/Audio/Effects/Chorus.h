#pragma once
#include "AudioEffect.h"

class Chorus : public Effect {
    enum { Time, Feedback, Pan, delaySize = 128 };

    int leftIndex, rightIndex;
    float delayTime, feedback, pan;

    float leftBuffer[delaySize]{};
    float rightBuffer[delaySize]{};

public:

    explicit Chorus(SingletonRepo* repo) : Effect(repo, "Chorus")
        ,	leftIndex(0)
        ,	rightIndex(0)
        ,	delayTime(0.5f)
        ,	feedback(0.5f)
        ,	pan(0.5f) {
        for (int i = 0; i < delaySize; ++i) {
            leftBuffer[i] = rightBuffer[i] = 0.f;
        }
    }

    void processBuffer(AudioSampleBuffer& buffer) override {
        int delaySamples = int(delayTime * delaySize);
        float added;

        for (int j = 0; j < buffer.getNumSamples(); ++j) {
            float& sample = *buffer.getWritePointer(0, j);
            added = feedback * pan * leftBuffer[(leftIndex - delaySamples) & (delaySize - 1)];
            leftBuffer[(leftIndex++) & (delaySize - 1)] = sample + added;
            sample += added * pan;
        }

        if (buffer.getNumChannels() > 1) {
            for (int j = 0; j < buffer.getNumSamples(); ++j) {
                float& sample = *buffer.getWritePointer(1, j);
                added = feedback * rightBuffer[(rightIndex - delaySamples) & (delaySize - 1)];
                rightBuffer[(rightIndex++) & (delaySize - 1)] = sample + added;
                sample += added * (1 - pan);
            }
        }
    }

    [[nodiscard]] bool isEnabled() const override {
        return false;
    }

private:
    void setDelayTime(float time) {
        delayTime = time;
    }

    void setFeedback(float feedback) {
        this->feedback = feedback;
    }

    void setPan(float pan) {
        this->pan = pan;
    }

public:
    void doParamChange(int param, float value, bool doFurtherUpdate) {
        switch (param) {
            case Time: 		setDelayTime(value); break;
            case Feedback: 	setFeedback(value);  break;
            case Pan:		setPan(value);		 break;
            default:
                throw std::invalid_argument("Invalid param " + std::to_string(param));
        }
    }
};
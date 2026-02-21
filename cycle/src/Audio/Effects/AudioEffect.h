#pragma once
#include <App/SingletonAccessor.h>
#include "JuceHeader.h"

class Effect :
    public SingletonAccessor
{
public:

    Effect(SingletonRepo* repo, const String& name) :
        SingletonAccessor(repo, name) {
    }

    void process(AudioSampleBuffer& buffer)	{
        audioThreadUpdate();

        if (! isEnabled()) {
            return;
        }

        processBuffer(buffer);
    }

    virtual void processBuffer(AudioSampleBuffer& buffer) = 0;
    [[nodiscard]] virtual bool isEnabled() const = 0;

    bool paramChanged(int index, double value, bool doFurtherUpdate) {
        return doParamChange(index, value, doFurtherUpdate);
    }

protected:
    virtual void audioThreadUpdate() {}
    virtual bool doParamChange(int index, double value, bool doFurtherUpdate) = 0;
};

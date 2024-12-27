#include <App/SingletonRepo.h>

#include "WaveShaper.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../UI/Effects/WaveshaperUI.h"
#include "../../UI/VisualDsp.h"
#include "../../Util/CycleEnums.h"

Waveshaper::Waveshaper(SingletonRepo* repo) : Effect(repo, "Waveshaper")
                                              , preamp(1.f)
                                              , postamp(1.f)
                                              , pendingOversampleFactor(1)
                                              , table(tableResolution) {
}

void Waveshaper::init() {
    rampBuffer.resize(getConstant(MaxBufferSize));
    graphicOversampleBuf.resize(getConstant(MaxCyclePeriod) / 2 * maxOversampleFactor);

    // 2 for stereo + 1 for graphics, separate for threading
    for(int i = 0; i < 3; ++i) {
        auto* oversampler = new Oversampler(repo, 16);
        oversampler->setOversampleFactor(1);
        oversamplers.add(oversampler);
    }

    // prevent audio thread contention with graphic processing
    oversamplers[graphicOvspIndex]->setMemoryBuf(graphicOversampleBuf);
}

void Waveshaper::rasterizeTable() {
    ScopedLock sl1(getObj(SynthAudioSource).getLock());

    int halfRes = tableResolution / 2;
    float padding = getRealConstant(WaveshaperPadding);
    double delta = (1 - 2 * padding) / double(tableResolution / 2 - 1);
    double phase = padding + 0.5 * delta;

    Buffer<float> halfTable(table + halfRes, halfRes);
    rasterizer->samplePerfectly(delta, halfTable, phase);

    halfTable.add(-padding).mul(1.f / (1.f - 2.f * padding));

    ippsFlip_32f(table + halfRes, table, halfRes);
    table.withSize(halfRes).mul(-1.f);
}

void Waveshaper::processBuffer(AudioSampleBuffer& audioBuffer) {
    int numSamples = audioBuffer.getNumSamples();

    if (numSamples == 0) {
        return;
    }

    for (int i = 0; i < audioBuffer.getNumChannels(); ++i) {
        Buffer<Ipp32f> buffer(audioBuffer, i);

        oversamplers[i]->start(buffer);

        preamp.maybeApplyRamp(rampBuffer.withSize(buffer.size()), buffer, 0.5);
        buffer.add(0.5f);

        ippsThreshold_LTValGTVal_32f_I(buffer, buffer.size(), 0.f, 0.f, 1.f, 1.f);

        for (float& j : buffer) {
            linInterpTable(j);
        }

        oversamplers[i]->stop();
        postamp.maybeApplyRamp(rampBuffer.withSize(buffer.size()), buffer);
    }
}

void Waveshaper::processVertexBuffer(Buffer<Ipp32f> outputBuffer) {
    bool doOversample = doesGraphicOversample();

    if (doOversample) {
        oversamplers[graphicOvspIndex]->start(outputBuffer);
    }

    int oversampSize = outputBuffer.size();

    outputBuffer.mul((float) preamp.getTargetValue()).add(0.5f);

    ippsThreshold_LTValGTVal_32f_I(outputBuffer, oversampSize, 0.f, 0.f, 1.f, 1.f);

    for (int i = 0; i < oversampSize; ++i) {
        linInterpTable(outputBuffer[i]);
    }

    if (doOversample) {
        oversamplers[graphicOvspIndex]->stop();
    }

    // keep the range to -0.5..0.5
    outputBuffer.mul((float) postamp.getTargetValue() * 0.5f);
}

bool Waveshaper::doesGraphicOversample() {
    return oversamplers[graphicOvspIndex]->getOversampleFactor() > 1;
    // TODO
    // && ! getObj(VisualDsp).getFXProcessor()->isDetailReduced();
}

bool Waveshaper::doParamChange(int param, double value, bool doFurtherUpdate) {
    switch (param) {
        case Postamp: return postamp.setTargetValue(calcPostamp(value));
        case Preamp: return preamp.setTargetValue(calcPreamp(value));
        default: break;
    }

    return true;
}

void Waveshaper::audioThreadUpdate() {
    if (pendingOversampleFactor > 0) {
        for (int i = 0; i < graphicOvspIndex; ++i) {
            oversamplers[i]->setOversampleFactor(pendingOversampleFactor);
        }

        pendingOversampleFactor = -1;
    }
}

void Waveshaper::setPendingOversampleFactor(int factor) {
    oversamplers[graphicOvspIndex]->setOversampleFactor(factor);
    pendingOversampleFactor = factor;
}

void Waveshaper::clearGraphicDelayLine() {
    oversamplers[graphicOvspIndex]->resetDelayLine();
}

int Waveshaper::getLatencySamples() {
    return (isEnabled() && oversamplers[graphicOvspIndex]->getOversampleFactor() > 1)
               ? oversamplers[graphicOvspIndex]->getLatencySamples()
               : 0;
}

void Waveshaper::updateSmoothedParameters(int deltaSamples) {
    postamp.update(deltaSamples);
    preamp.update(deltaSamples);
}

bool Waveshaper::isEnabled() const {
    return ui->isEffectEnabled();
}

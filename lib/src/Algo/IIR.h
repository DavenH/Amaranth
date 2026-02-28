#pragma once

#include "JuceHeader.h"

#include <Array/Buffer.h>
#include <Array/ScopedAlloc.h>
#include <Audio/Filters/Filter.h>
#include <vector>

#if defined(USE_IPP) && !defined(__APPLE__)
  #include <ipp.h>
#endif

class IIR
{
public:
    static constexpr int tapsPerBiquad = 6;

    explicit IIR(int channels = 1) :
        numChannels(jmax(1, channels))
    {
       #if defined(USE_IPP) && !defined(__APPLE__)
        ippStates.resize((size_t) numChannels, nullptr);
        stateSizes.resize((size_t) numChannels, 0);
        taps.resize((size_t) numChannels);
        delayLines.resize((size_t) numChannels);
       #else
        juceStages.resize((size_t) numChannels);
       #endif
    }

    template<typename CascadeType>
    void updateFromCascade(CascadeType& cascade)
    {
        const int stageCount = cascade.getNumStages();

       #if defined(USE_IPP) && !defined(__APPLE__)
        prepareIppStorage(stageCount);
        updateIppTaps(cascade);
       #else
        prepareJuceStages(stageCount);
        updateJuceCoefficients(cascade);
       #endif
    }

    void process(int channel, Buffer<Float32> samples)
    {
        jassert(channel >= 0 && channel < numChannels);

        if (numStages == 0 || samples.size() == 0) {
            return;
        }

       #if defined(USE_IPP) && !defined(__APPLE__)
        jassert(ippStates[(size_t) channel] != nullptr);
        ippsIIR64f_32f(samples, samples, samples.size(), ippStates[(size_t) channel]);
       #else
        Float32* channelData[] = { samples.get() };
        juce::dsp::AudioBlock<Float32> block(channelData, (size_t) 1, (size_t) samples.size());

        for (auto& stage : juceStages[(size_t) channel]) {
            juce::dsp::ProcessContextReplacing<Float32> context(block);
            stage.process(context);
        }
       #endif
    }

    void clear()
    {
       #if defined(USE_IPP) && !defined(__APPLE__)
        if (numStages == 0) {
            return;
        }

        delayStorage.zero();
        initialiseIppStates();
       #else
        for (auto& channelStages : juceStages) {
            for (auto& stage : channelStages) {
                stage.reset();
            }
        }
       #endif
    }

private:
    int numChannels = 1;
    int numStages = 0;

   #if defined(USE_IPP) && !defined(__APPLE__)
    ScopedAlloc<Float64> tapsStorage;
    ScopedAlloc<Float64> delayStorage;
    ScopedAlloc<Int8u> stateStorage;
    std::vector<Buffer<Float64>> taps;
    std::vector<Buffer<Float64>> delayLines;
    std::vector<IppsIIRState64f_32f*> ippStates;
    std::vector<int> stateSizes;

    void prepareIppStorage(int stageCount)
    {
        if (stageCount <= 0) {
            numStages = 0;
            return;
        }

        const bool hasStageCountChanged = numStages != stageCount;
        const int tapsLength = stageCount * tapsPerBiquad;
        const int delayLength = stageCount * 2;

        if (hasStageCountChanged) {
            tapsStorage.resize(numChannels * tapsLength);
            delayStorage.resize(numChannels * delayLength);
            delayStorage.zero();

            int totalStateBytes = 0;
            for (int channel = 0; channel < numChannels; ++channel) {
                int stateSize = 0;
                ippsIIRGetStateSize64f_BiQuad_32f(stageCount, &stateSize);
                stateSizes[(size_t) channel] = stateSize;
                totalStateBytes += stateSize;
            }

            stateStorage.resize(totalStateBytes);
            numStages = stageCount;
        }

        for (int channel = 0; channel < numChannels; ++channel) {
            const int tapsOffset = channel * tapsLength;
            const int delayOffset = channel * delayLength;
            taps[(size_t) channel] = Buffer<Float64>(tapsStorage + tapsOffset, tapsLength);
            delayLines[(size_t) channel] = Buffer<Float64>(delayStorage + delayOffset, delayLength);
        }
    }

    template<typename CascadeType>
    void updateIppTaps(CascadeType& cascade)
    {
        if (numStages == 0) {
            return;
        }

        const int tapsLength = numStages * tapsPerBiquad;
        Buffer<Float64> firstChannelTaps = taps[(size_t) 0];
        int tapIndex = 0;

        for (int stage = 0; stage < numStages; ++stage) {
            const auto& stageData = cascade[stage];
            const Float64 stageTaps[] = {
                stageData.m_b0, stageData.m_b1, stageData.m_b2,
                stageData.m_a0, stageData.m_a1, stageData.m_a2
            };

            for (int i = 0; i < tapsPerBiquad; ++i) {
                firstChannelTaps[tapIndex + i] = stageTaps[i];
            }

            tapIndex += tapsPerBiquad;
        }

        for (int channel = 1; channel < numChannels; ++channel) {
            firstChannelTaps.copyTo(taps[(size_t) channel]);
        }

        initialiseIppStates();
    }

    void initialiseIppStates()
    {
        if (numStages == 0) {
            return;
        }

        int stateOffset = 0;
        for (int channel = 0; channel < numChannels; ++channel) {
            Buffer<Int8u> stateChunk(stateStorage + stateOffset, stateSizes[(size_t) channel]);
            ippsIIRInit64f_BiQuad_32f(&ippStates[(size_t) channel],
                                      taps[(size_t) channel],
                                      numStages,
                                      delayLines[(size_t) channel],
                                      stateChunk);
            stateOffset += stateSizes[(size_t) channel];
        }
    }

   #else
    std::vector<std::vector<juce::dsp::IIR::Filter<Float32>>> juceStages;

    void prepareJuceStages(int stageCount)
    {
        if (stageCount <= 0) {
            numStages = 0;
            return;
        }

        if (numStages != stageCount) {
            for (auto& channelStages : juceStages) {
                channelStages.resize((size_t) stageCount);
            }

            numStages = stageCount;
            clear();
        }
    }

    template<typename CascadeType>
    void updateJuceCoefficients(CascadeType& cascade)
    {
        if (numStages == 0) {
            return;
        }

        for (int stage = 0; stage < numStages; ++stage) {
            const auto& stageData = cascade[stage];
            juce::dsp::IIR::Coefficients<Float32> coeffs(
                (Float32) stageData.m_b0,
                (Float32) stageData.m_b1,
                (Float32) stageData.m_b2,
                (Float32) stageData.m_a0,
                (Float32) stageData.m_a1,
                (Float32) stageData.m_a2
            );

            for (auto& channelStages : juceStages) {
                *channelStages[(size_t) stage].coefficients = coeffs;
            }
        }
    }
   #endif
};

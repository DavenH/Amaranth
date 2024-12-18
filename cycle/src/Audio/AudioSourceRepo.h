#pragma once

#include <App/SingletonAccessor.h>
#include <Audio/AudioHub.h>
#include <Obj/Ref.h>
#include "JuceHeader.h"

class WavAudioSource;
class SynthAudioSource;
class GraphicTableAudioSource;

class AudioSourceRepo:
        public SingletonAccessor
    ,	public AudioHub::SettingListener
{
public:
    enum AudioSourceEnum { WavSource, SynthSource, TableSource, NullSource };

    void init() override;

    ~AudioSourceRepo() override = default;
    explicit AudioSourceRepo(SingletonRepo* repo);

    [[nodiscard]] Ref<GraphicTableAudioSource> getTableAudioSource() const { return tableSource.get(); }
    [[nodiscard]] Ref<WavAudioSource> getWavAudioSource() const { return wavSource.get(); }

    void setAudioProcessor(AudioSourceEnum source);

private:
    Ref<AudioHub> audioHub;
    Ref<SynthAudioSource> synthSource;

    std::unique_ptr<GraphicTableAudioSource> tableSource;
    std::unique_ptr<WavAudioSource> wavSource;
};

#include <App/SingletonRepo.h>
#include <Test/CsvFile.h>
#include "GraphicTableAudioSource.h"
#include "SynthAudioSource.h"
#include "Voices/GraphicSynthVoice.h"
#include "../Audio/AudioSourceRepo.h"
#include "../Audio/Effects/Delay.h"
#include "../Audio/Effects/Reverb.h"
#include "../Audio/SynthAudioSource.h"

GraphicTableAudioSource::GraphicTableAudioSource(SingletonRepo* repo) : SingletonAccessor(
    repo, "GraphicTableAudioSource") {
    initVoices();

    delay = &getObj(SynthAudioSource).getDelay();
    reverb = &getObj(SynthAudioSource).getReverb();
}

void GraphicTableAudioSource::initVoices() {
    synth.addSound(new GraphicSynthSound());

    for (int i = 0; i < numTableVoices; ++i)
        synth.addVoice(new GraphicSynthVoice(repo));
}

void GraphicTableAudioSource::releaseResources() {
}

void GraphicTableAudioSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void GraphicTableAudioSource::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) {
    ScopedLock lock(audioLock);

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    //	delay->process(buffer);
    //	reverb->process(buffer);
}

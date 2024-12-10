#include <Algo/AutoModeller.h>
#include <Algo/PitchTracker.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/Multisample.h>
#include <Audio/PluginProcessor.h>
#include <Thread/LockTracer.h>
#include <UI/IConsole.h>
#include <Util/NumberUtils.h>

#include "AudioSourceRepo.h"
#include "GraphicTableAudioSource.h"
#include "SynthAudioSource.h"
#include "WavAudioSource.h"

#include <iterator>
#include <algorithm>
#include <deque>
#include <fstream>


AudioSourceRepo::AudioSourceRepo(SingletonRepo* repo) :
		SingletonAccessor(repo, "AudioSourceRepo") {
	getObj(AudioHub).addListener(this);
}


AudioSourceRepo::~AudioSourceRepo() {
}

void AudioSourceRepo::init() {
	synthSource = &getObj(SynthAudioSource);
	audioHub 	= &getObj(AudioHub);
	wavSource 	= new WavAudioSource(repo);
}


void AudioSourceRepo::setAudioProcessor(AudioSourceEnum source) {
    AudioSourceProcessor* proc = audioHub->getAudioSourceProcessor();
	ScopedPointer<ScopedLock> sl;

	if(proc != nullptr)
		sl = new ScopedLock(proc->getLock());

    switch (source) {
        case WavSource: {
			audioHub->setAudioSourceProcessor(wavSource);
			wavSource->reset();
			break;
		}
        case SynthSource: {
			audioHub->setAudioSourceProcessor(synthSource);
			synthSource->doAudioThreadUpdates();
			break;
		}
		case NullSource:
		default:
			audioHub->setAudioSourceProcessor(nullptr);
	}
}


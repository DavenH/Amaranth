#include <App/SingletonRepo.h>

#include "Synthesizer.h"
#include "../Audio/SynthAudioSource.h"
#include "../UI/Panels/ModMatrixPanel.h"
#include "../UI/Panels/Morphing/MorphPanel.h"


void Synthesizer::handleController(int midiChannel,
                                   int controllerNumber,
                                   int controllerValue) {
    if (controllerNumber < VolumeParam) {
        float unsaturatedValue = controllerValue / float(127);
        getObj(ModMatrixPanel).route(unsaturatedValue, ModMatrixPanel::MidiController + controllerNumber);

        if (controllerNumber == ModWheel) {
            getObj(SynthAudioSource).setLastBlueLevel(unsaturatedValue);
        }
    } else {
        for (int i = 0; i < getNumVoices(); ++i) {
            SynthesiserVoice* voice = getVoice(i);
            voice->controllerMoved(controllerNumber, controllerValue);
        }
    }
}

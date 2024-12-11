#include <ipp.h>
#include <App/AppConstants.h>
#include <App/SingletonRepo.h>
#include <Array/Column.h>

#include "GraphicSynthVoice.h"
#include "../../UI/Panels/PlaybackPanel.h"
#include "../../UI/VisualDsp.h"
#include "../../UI/Panels/OscControlPanel.h"
#include "../../Util/CycleEnums.h"
#include <Definitions.h>

GraphicSynthVoice::GraphicSynthVoice(SingletonRepo* repo) :
        CycleBasedVoice	(nullptr, repo)
    ,	columns			(nullptr)
    ,	renderBuffer	(2)
    ,	increment		(0)
    ,	currentAngle	(0)
    ,	playing			(false)
    ,	leftMemory		(getConstant(MaxBufferSize))
    ,	rightMemory		(getConstant(MaxBufferSize))
{
    cycleCompositeAlgo = Chain;
}

void GraphicSynthVoice::startNote(const int midiNoteNumber,
                                  const float velocity,
                                  SynthesiserSound* sound,
                                  const int currentPitchWheelPosition) {
    if (getSetting(CurrentMorphAxis) != Vertex::Time) {
        stop(false);
        return;
    }

    jassert(midiNoteNumber >= getConstant(LowestMidiNote));

    groups[0].reset();
    noteState.scratchVoiceTime = getObj(PlaybackPanel).getX();
    float speedScale = 1 / getObj(OscControlPanel).getLengthInSeconds();

    int displayKey = midiNoteNumber - 12;

    groups[0].angleDelta = MidiMessage::getMidiNoteInHertz(displayKey) / 44100.0;
    increment = speedScale / (44100.f * (float) groups[0].angleDelta);
    currentAngle = 0;
    noteState.totalSamplesPlayed = 0;

    initCycleBuffers();

    columns = &getObj(VisualDsp).getTimeColumns();

    if (columns->empty()) {
        playing = false;
        return;
    }

    playing = true;
}

void GraphicSynthVoice::stopNote(float velocity, bool allowTailOff) {
    playing = false;
    clearCurrentNote();
}

bool GraphicSynthVoice::canPlaySound(SynthesiserSound* sound) {
    return true;
}

void GraphicSynthVoice::pitchWheelMoved(const int newValue) {
}

void GraphicSynthVoice::controllerMoved(const int controllerNumber, const int newValue) {
}

void GraphicSynthVoice::renderNextBlock(AudioSampleBuffer& outputBuffer, int startSample, int numSamples) {
    if (!playing) {
        return;
    }

    //	leftMemory.ensureSize(numSamples);
    //	rightMemory.ensureSize(numSamples);

    renderBuffer.numChannels = outputBuffer.getNumChannels();

    for (int i = 0; i < renderBuffer.numChannels; ++i) {
        renderBuffer[i] = Buffer<float>(outputBuffer.getWritePointer(i) + startSample, numSamples);
    }

    render(renderBuffer);
}

void GraphicSynthVoice::incrementCurrentX() {
}

void GraphicSynthVoice::calcCycle(VoiceParameterGroup& group) {
    ScopedLock lock(getObj(VisualDsp).getCalculationLock());

    int colVectorSize = columns->size();

    if (colVectorSize < 2) {
        return;
    }

    int samplesThisCycle = group.samplesThisCycle;

    float position = noteState.scratchVoiceTime * colVectorSize;
    float nextPos = (noteState.scratchVoiceTime + increment) * colVectorSize;

    int columnIndexA = jmin(colVectorSize - 1, int(position));
    int columnIndexB = jmin(colVectorSize - 1, int(nextPos));
    int colSize = columns->front().size();

    float columnProgress = 0;

    for (int i = 0; i < samplesThisCycle; ++i) {
        float realSampIndex = (float) currentAngle * colSize;
        int sampIndex = int(realSampIndex);
        float sampleRem = realSampIndex - sampIndex;

        sampIndex &= (colSize - 1);
        int nextSampIndex = int(sampIndex + 1) & (colSize - 1);

        currentAngle += groups[0].angleDelta;

		float columnAVal = (1 - sampleRem) * (*columns)[columnIndexA][sampIndex] + sampleRem * (*columns)[columnIndexA][nextSampIndex];
		float columnBVal = (1 - sampleRem) * (*columns)[columnIndexB][sampIndex] + sampleRem * (*columns)[columnIndexB][nextSampIndex];

        columnProgress = i / float(samplesThisCycle);
        layerAccumBuffer[Left][i] = (1 - columnProgress) * columnAVal + columnProgress * columnBVal;
    }

    layerAccumBuffer[Left]
        .section(0, samplesThisCycle)
        .mul(2)
        .copyTo(layerAccumBuffer[Right]);

    noteState.scratchVoiceTime += increment;

    if (noteState.scratchVoiceTime > 1) {
        noteState.scratchVoiceTime = 1.f;
    }
}

void GraphicSynthVoice::updateCycleVariables(int groupIndex, int numSamples) {
}

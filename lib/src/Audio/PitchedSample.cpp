#include "PitchedSample.h"

#include <memory>
#include "../Algo/AutoModeller.h"
#include "../App/AppConstants.h"
#include "../Wireframe/Rasterizer/EnvRasterizer.h"
#include "../Util/Arithmetic.h"
#include "../Util/NumberUtils.h"
#include "../Wireframe/OldMeshRasterizer.h"
#include "../Util/Util.h"
#include "../Definitions.h"

PitchedSample::PitchedSample() :
        samplerate  (44100)
    ,   fundNote    (-1)
    ,   playbackPos (0)
    ,   audio       (2)
    ,   phaseOffset (0.f) {

    midiLimits  = Range<int>(Constants::LowestMidiNote, Constants::HighestMidiNote);
    veloRange   = Range<float>(0, 1.f);
    midiRange   = midiLimits;

    mesh = std::make_unique<Mesh>();
}

PitchedSample::PitchedSample(const Buffer<float>& sample) :
        PitchedSample() {
    audio = StereoBuffer(sample);
}

void PitchedSample::writeXML(XmlElement* sampleElem) const {
    sampleElem->setAttribute("path",        lastLoadedFilePath);
    sampleElem->setAttribute("fund-note",   fundNote);

    sampleElem->setAttribute("midi-start",  midiRange.getStart());
    sampleElem->setAttribute("midi-end",    midiRange.getEnd());

    sampleElem->setAttribute("vel-start",   veloRange.getStart());
    sampleElem->setAttribute("vel-end",     veloRange.getEnd());
    sampleElem->setAttribute("phase-offset",phaseOffset);

    mesh->writeXML(sampleElem);
}

bool PitchedSample::readXML(const XmlElement* sampleElem) {
    File file = File(sampleElem->getStringAttribute("path"));
    fundNote    = sampleElem->getIntAttribute("fund-note",           60);
    phaseOffset = sampleElem->getDoubleAttribute("phase-offset",     0.f);

    midiRange.setStart  (sampleElem->getIntAttribute("midi-start",   60));
    midiRange.setEnd    (sampleElem->getIntAttribute("midi-end",     60));

    veloRange.setStart  (sampleElem->getDoubleAttribute("vel-start", 0.f));
    veloRange.setEnd    (sampleElem->getDoubleAttribute("vel-end",   1.f));

    mesh->readXML(sampleElem);

    if(load(file.getFullPathName()) < 0) {
        return false;
    }

    return true;
}

PitchedSample::~PitchedSample() {
    clear();
    mesh->destroy();
}

void PitchedSample::createDefaultPeriods() {
    float period = 1024;

    if(fundNote > 10) {
        period = samplerate / NumberUtils::noteToFrequency(fundNote);
    }

    const int maxSamples = 200;
    int cume = 0;

    periods.clear();

    float increment = jmax(audio.size() / float(maxSamples), period);

    for (; cume < audio.size(); cume += increment) {
        PitchFrame frame;

        frame.period        = period;
        frame.sampleOffset  = cume;

        periods.push_back(frame);
    }
}


void PitchedSample::createEnvFromPeriods(bool isMulti) {
    if (periods.size() < 3 || size() < 2048) {
        return;
    }

    const int maxLines  = 300;
    float isamples      = 1.f / (float) size();
    float averageFreq   = NumberUtils::noteToFrequency(fundNote);
    float lastPeriod    = samplerate / averageFreq;
    int decimFactor     = jmax(1, (int) periods.size() / maxLines);

    vector<Vertex2> path;
    ScopedAlloc<float> levels(periods.size() / decimFactor);

    for (int i = 0; i < periods.size() - (decimFactor - 1); i += decimFactor) {
        PitchFrame& frame = periods[i];
        float period     = frame.period > 10 ? frame.period : lastPeriod;
        lastPeriod       = period;

        float semisAbove = NumberUtils::frequencyToNote(samplerate / period) - fundNote;
        float yValue     = NumberUtils::semisToUnitPitch(semisAbove);

        NumberUtils::constrain(yValue, 0.f, 1.f);

        path.emplace_back(frame.sampleOffset * isamples, yValue);
    }

    jassert(mesh != nullptr);
    mesh->destroy();

    float thresh = isMulti ? 0.002 : 0.0005;
    vector<Vertex2> path2 = AutoModeller::reducePath(path, thresh);
    for (auto& i : path2) {
        auto* vertex = new Vertex(i.x, i.y);
        vertex->values[Vertex::Curve] = 0.1f;

        mesh->addVertex(vertex);
    }
}

void PitchedSample::createPeriodsFromEnv(OldMeshRasterizer* rast) {
    jassert(fundNote > 0);
    progressMark

    if(fundNote < 0) {
        return;
    }

    int currentIndex = 0;
    float position = 0;
    float defaultFreq = NumberUtils::noteToFrequency(fundNote, 0);

    rast->setMesh(mesh.get());
    rast->generateControlPoints();
    rast->storeRasteredArrays();

    if(rast->isSampleable())
    {
        periods.clear();

        int sz          = size();
        int maxPeriods  = 400;
        float period    = samplerate / defaultFreq;
        int decimFactor = jmax(1, sz / int (maxPeriods * period));

        while ((int) position < sz) {
            float x = position / float(sz);

            if (rast->isSampleableAt(x)) {
                float y = rast->sampleAt(x, currentIndex);

                NumberUtils::constrain(y, 0.1f, 0.9f);
                float value = NumberUtils::unitPitchToSemis(y);

                float freq = NumberUtils::noteToFrequency(fundNote, 100 * value);
                NumberUtils::constrain(freq, 20.f, 2500.f);

                period = samplerate / freq;
            }

            periods.emplace_back(position, period, 0.2f);
            position += period * decimFactor;
        }
    }
}

void PitchedSample::shiftOctave(bool up) {
    if (fundNote < 0 || periods.empty()) {
        return;
    }

    int old = fundNote;
    int offset = 12 * (up ? 1 : -1);
    fundNote += offset;
    midiRange += offset;

    if (!NumberUtils::within(fundNote, midiLimits)) {
        fundNote = old;
        throw std::runtime_error("Desired midi note is out of range.");
    }

    for (auto& period : periods)
        period.period *= (up ? 0.5f : 2.f);
}

MorphPosition PitchedSample::getAveragePosition() const {
    float avgNote = (midiRange.getStart() + midiRange.getEnd()) * 0.5f;
    float red     = Arithmetic::getUnitValueForNote(roundToInt(avgNote), midiRange);
    float avgVel  = (veloRange.getStart() + veloRange.getEnd()) * 0.5f;
    float blue    = 1 - avgVel;

    return {0, red, blue};
}

MorphPosition PitchedSample::getBox() {
    MorphPosition m;
    m.red       = Arithmetic::getUnitValueForNote(midiRange.getStart(), midiLimits);
    m.redDepth  = Arithmetic::getUnitValueForNote(midiRange.getEnd(), midiLimits) - m.red;

    m.blue      = 1.f - veloRange.getEnd();
    m.blueDepth = veloRange.getEnd() - veloRange.getStart();

    m.time      = 0;
    m.timeDepth = lengthSeconds();

    return m;
}

float PitchedSample::getAveragePeriod() {
    float average = 0;

    for (auto& period : periods) {
        average += period.period;
    }

    if (periods.empty()) {
        if(fundNote > 10) {
            return samplerate / NumberUtils::noteToFrequency(fundNote);
        }
    } else {
        return average / float(periods.size());
    }

    return 1024.f;
}

int PitchedSample::load(const String& filename) {
    File audioFile(File::getCurrentWorkingDirectory().getChildFile(filename).getFullPathName());

    if(! audioFile.existsAsFile()) {
        return -3;
    }

    std::unique_ptr<AudioFormatReader> reader;
    std::unique_ptr stream(audioFile.createInputStream());

    if (stream == nullptr) {
        return -3;
    }

    String ext(audioFile.getFileExtension());

    if (ext.equalsIgnoreCase(".wav")) {
        reader.reset(WavAudioFormat().createReaderFor(stream.release(), true));
    } else if (ext.containsIgnoreCase("aif")) {
        reader.reset(AiffAudioFormat().createReaderFor(stream.release(), true));
    } else if (ext.equalsIgnoreCase(".mp3")) {
      #if JUCE_USE_MP3AUDIOFORMAT
        reader.reset(MP3AudioFormat().createReaderFor(stream.release(), true));
      #else
        return -2;
      #endif
    } else if (ext.equalsIgnoreCase(".ogg")) {
        reader.reset(OggVorbisAudioFormat().createReaderFor(stream.release(), true));
    }

    if (reader == nullptr) {
        return -1;
    }

    int size = (int) jmin(2 * 1024 * 1024LL, reader->lengthInSamples);
    samplerate = int(reader->sampleRate);

    AudioBuffer<float> audioBuffer;
    audioBuffer.setSize(reader->numChannels, size);
    reader->read(&audioBuffer, 0L, size, 0L, true, true);

    lastLoadedFilePath = audioFile.getFullPathName();
    audio = StereoBuffer(audioBuffer);
    audio.takeOwnership();

    float maxMag = audio.max();

    if (maxMag != 0) {
        audio.mul(1.f / maxMag);
    }

    return 0;
}

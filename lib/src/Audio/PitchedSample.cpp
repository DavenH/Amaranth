#include "PitchedSample.h"

#include <memory>
#include "../Algo/AutoModeller.h"
#include "../App/AppConstants.h"
#include "../App/Doc/PresetJson.h"
#include "../App/MeshLibrary.h"
#include "../Curve/Rasterization/Rasterizer/EnvRasterizer.h"
#include "../Curve/Rasterization/Rasterizer/FXRasterizer.h"
#include "../Curve/Mesh/Mesh.h"
#include "../Util/Arithmetic.h"
#include "../Util/NumberUtils.h"
#include "../Util/Util.h"
#include "../Definitions.h"

PitchedSample::PitchedSample() :
        samplerate  (44100)
    ,   fundNote    (-1)
    ,   meshLayerIndex(CommonEnums::Null)
    ,   playbackPos (0)
    ,   audio       (2)
    ,   phaseOffset (0.f) {

    midiLimits  = Range<int>(Constants::LowestMidiNote, Constants::HighestMidiNote);
    veloRange   = Range<float>(0, 1.f);
    midiRange   = midiLimits;
}

PitchedSample::PitchedSample(const Buffer<float>& sample) :
        PitchedSample() {
    copyAudio(sample);
}

void PitchedSample::copyAudio(const Buffer<float>& source) {
    audioMemory.resize(source.size());
    source.copyTo(audioMemory);
    audio = StereoBuffer(audioMemory);
}

void PitchedSample::copyAudio(AudioBuffer<float>& source) {
    const int numChannels = jmin(2, source.getNumChannels());
    const int channelSize = source.getNumSamples();
    audioMemory.resize(numChannels * channelSize);
    audioMemory.resetPlacement();
    audio = StereoBuffer(numChannels);

    for (int channel = 0; channel < numChannels; ++channel) {
        Buffer<float> destination = audioMemory.place(channelSize);
        Buffer<float>(source, channel).copyTo(destination);
        audio[channel] = destination;
    }
}

void PitchedSample::writeXML(XmlElement* sampleElem) const {
    sampleElem->setAttribute("path",        lastLoadedFilePath);
    sampleElem->setAttribute("fund-note",   fundNote);

    sampleElem->setAttribute("midi-start",  midiRange.getStart());
    sampleElem->setAttribute("midi-end",    midiRange.getEnd());

    sampleElem->setAttribute("vel-start",   veloRange.getStart());
    sampleElem->setAttribute("vel-end",     veloRange.getEnd());
    sampleElem->setAttribute("phase-offset",phaseOffset);
    sampleElem->setAttribute("mesh-layer-index", meshLayerIndex);
}

bool PitchedSample::readXML(const XmlElement* sampleElem) {
    File file = File(sampleElem->getStringAttribute("path"));
    fundNote    = sampleElem->getIntAttribute("fund-note",           60);
    phaseOffset = sampleElem->getDoubleAttribute("phase-offset",     0.f);

    midiRange.setStart  (sampleElem->getIntAttribute("midi-start",   60));
    midiRange.setEnd    (sampleElem->getIntAttribute("midi-end",     60));

    veloRange.setStart  (sampleElem->getDoubleAttribute("vel-start", 0.f));
    veloRange.setEnd    (sampleElem->getDoubleAttribute("vel-end",   1.f));
    meshLayerIndex = sampleElem->getIntAttribute("mesh-layer-index", CommonEnums::Null);

    if(load(file.getFullPathName()) < 0) {
        return false;
    }

    return true;
}

PitchedSample::~PitchedSample() {
    clear();
}

var PitchedSample::writeJSON() const {
    auto json = PresetJson::object();

    json->setProperty("path", lastLoadedFilePath);
    json->setProperty("fundNote", fundNote);
    json->setProperty("midiStart", midiRange.getStart());
    json->setProperty("midiEnd", midiRange.getEnd());
    json->setProperty("velStart", veloRange.getStart());
    json->setProperty("velEnd", veloRange.getEnd());
    json->setProperty("phaseOffset", phaseOffset);
    json->setProperty("meshLayerIndex", meshLayerIndex);

    return PresetJson::toVar(json);
}

bool PitchedSample::readJSON(const var& object) {
    if (PresetJson::getObject(object) == nullptr) {
        return false;
    }

    lastLoadedFilePath = PresetJson::stringProperty(object, "path", lastLoadedFilePath);
    fundNote = PresetJson::intProperty(object, "fundNote", fundNote);
    phaseOffset = PresetJson::doubleProperty(object, "phaseOffset", phaseOffset);
    meshLayerIndex = PresetJson::intProperty(object, "meshLayerIndex", meshLayerIndex);

    midiRange.setStart(PresetJson::intProperty(object, "midiStart", midiRange.getStart()));
    midiRange.setEnd(PresetJson::intProperty(object, "midiEnd", midiRange.getEnd()));
    veloRange.setStart(PresetJson::doubleProperty(object, "velStart", veloRange.getStart()));
    veloRange.setEnd(PresetJson::doubleProperty(object, "velEnd", veloRange.getEnd()));

    if (lastLoadedFilePath.isEmpty()) {
        return false;
    }

    return load(lastLoadedFilePath) >= 0;
}

Mesh* PitchedSample::getMesh(MeshLibrary& meshLibrary) const {
    int wavePitchGroupId = meshLibrary.getGroupBindings().wavePitch;

    if (wavePitchGroupId == CommonEnums::Null) {
        return nullptr;
    }

    auto& waveGroup = meshLibrary.getLayerGroup(wavePitchGroupId);

    if (!isPositiveAndBelow(meshLayerIndex, waveGroup.size())) {
        return nullptr;
    }

    return meshLibrary.getMesh(wavePitchGroupId, meshLayerIndex);
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


void PitchedSample::createEnvFromPeriods(MeshLibrary& meshLibrary, bool isMulti) {
    if (periods.size() < 3 || size() < 2048) {
        return;
    }

    Mesh* mesh = getMesh(meshLibrary);

    if (mesh == nullptr) {
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

    mesh->destroy();

    float thresh = isMulti ? 0.002 : 0.0005;
    vector<Vertex2> path2 = AutoModeller::reducePath(path, thresh);
    for (auto& i : path2) {
        auto* vertex = new Vertex(i.x, i.y);
        vertex->values[Vertex::Curve] = 0.1f;

        mesh->addVertex(vertex);
    }
}

namespace {
    void logPeriodRebuild(const PitchedSample& sample,
                          const char* context,
                          bool sampleable,
                          int oldCount,
                          float oldAverage) {
        float minPeriod = 1.e30f;
        float maxPeriod = 0.f;
        float average = 0.f;

        for (const auto& frame: sample.periods) {
            minPeriod = jmin(minPeriod, frame.period);
            maxPeriod = jmax(maxPeriod, frame.period);
            average += frame.period;
        }

        if (sample.periods.empty()) {
            minPeriod = 0.f;
        } else {
            average /= float(sample.periods.size());
        }

        String name = sample.lastLoadedFilePath.isNotEmpty()
            ? sample.lastLoadedFilePath
            : sample.uniqueName;

        DBG("PitchedSample periods"
            + String(" context=") + context
            + " sample=\"" + name + "\""
            + " sampleable=" + String(sampleable ? 1 : 0)
            + " oldFrames=" + String(oldCount)
            + " frames=" + String((int) sample.periods.size())
            + " oldAvgPeriod=" + String(oldAverage, 3)
            + " avgPeriod=" + String(average, 3)
            + " minPeriod=" + String(minPeriod, 3)
            + " maxPeriod=" + String(maxPeriod, 3)
            + " fundNote=" + String(sample.fundNote));
    }

    template<class RasterizerType>
    void createPeriodsFromEnv(
            PitchedSample& sample,
            MeshLibrary& meshLibrary,
            RasterizerType* rasterizer) {
        jassert(sample.fundNote > 0);

        if (sample.fundNote < 0 || rasterizer == nullptr) {
            return;
        }

        int currentIndex = 0;
        float position = 0;
        float defaultFreq = NumberUtils::noteToFrequency(sample.fundNote, 0);
        int oldCount = (int) sample.periods.size();
        float oldAverage = sample.getAveragePeriod();
        Mesh* mesh = sample.getMesh(meshLibrary);

        if (mesh == nullptr) {
            logPeriodRebuild(sample, "missing-mesh", false, oldCount, oldAverage);
            return;
        }

        rasterizer->setMesh(mesh);
        rasterizer->performUpdate(Update);

        auto sampler = rasterizer->sampler();
        if (sampler.isSampleable()) {
            sample.periods.clear();

            int sz          = sample.size();
            float period    = sample.samplerate / defaultFreq;

            while ((int) position < sz) {
                float x = position / float(sz);

                if (sampler.isSampleableAt(x)) {
                    float y = sampler.sampleAt(x, currentIndex);

                    NumberUtils::constrain(y, 0.1f, 0.9f);
                    float value = NumberUtils::unitPitchToSemis(y);

                    float freq = NumberUtils::noteToFrequency(sample.fundNote, 100 * value);
                    NumberUtils::constrain(freq, 20.f, 2500.f);

                    period = sample.samplerate / freq;
                }

                sample.periods.emplace_back(position, period, 0.2f);
                position += period;
            }
        }

        logPeriodRebuild(sample, "from-env", sampler.isSampleable(), oldCount, oldAverage);
    }
}

void PitchedSample::createPeriodsFromEnv(
        MeshLibrary& meshLibrary,
        EnvRasterizer* rasterizer) {
    progressMark
    ::createPeriodsFromEnv(*this, meshLibrary, rasterizer);
}

void PitchedSample::createPeriodsFromEnv(
        MeshLibrary& meshLibrary,
        FXRasterizer* rasterizer) {
    progressMark
    ::createPeriodsFromEnv(*this, meshLibrary, rasterizer);
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
    copyAudio(audioBuffer);

    float maxMag = audio.max();

    if (maxMag != 0) {
        audio.mul(1.f / maxMag);
    }

    return 0;
}

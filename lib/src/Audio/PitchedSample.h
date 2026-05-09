#pragma once
#include <vector>
#include "JuceHeader.h"
#include "../Array/StereoBuffer.h"
#include "../Obj/MorphPosition.h"
#include "../App/Doc/Savable.h"
#include "../Curve/Rasterization/Interfaces/MeshBindableRasterizer.h"
#include "../Curve/Rasterization/Interfaces/RasterizerSampler.h"
#include "../Curve/Rasterization/Interfaces/RasterizerUpdateTarget.h"
#include "../Util/CommonEnums.h"

using std::vector;

class SingletonRepo;
class Mesh;
class MeshLibrary;

class PitchFrame {
public:
    PitchFrame() : sampleOffset(0), period(0), atonal(0) {}
    PitchFrame(int offset, float period, float aperiod) :
        sampleOffset(offset), period(period), atonal(aperiod) {
    }

    int sampleOffset;
    float period;
    float atonal;

private:
    JUCE_LEAK_DETECTOR(PitchFrame)
};

typedef vector<PitchFrame>::iterator PitchIter;

class PitchedSample : public Savable {
public:
    PitchedSample();
    explicit PitchedSample(const Buffer<float>& source);
    ~PitchedSample() override;

    void clear() {
        audio.clear();
        periods.clear();
    }

    StereoBuffer read(int numSamples) {
        int oldPos = playbackPos;
        playbackPos += numSamples;

        return audio.section(oldPos, numSamples);
    }

    void resetPeriods()                         { periods.clear(); }
    void resetPosition()                        { playbackPos = 0; }
    [[nodiscard]] int size() const              { return audio.size(); }
    [[nodiscard]] float lengthSeconds() const   { return float(audio.size()) / float(samplerate); }

    void addFrame(const PitchFrame& frame) { periods.push_back(frame); }

    float getAveragePeriod();
    void createDefaultPeriods();
    [[nodiscard]] MorphPosition getAveragePosition() const;
    MorphPosition getBox();

    void createEnvFromPeriods(MeshLibrary& meshLibrary, bool isMulti);
    void createPeriodsFromEnv(
            MeshLibrary& meshLibrary,
            Rasterization::MeshBindableRasterizer* meshRasterizer,
            Rasterization::RasterizerUpdateTarget* updateTarget,
            Rasterization::RasterizerSampler* sampler);
    void shiftOctave(bool up);

    void writeXML(XmlElement* element) const override;
    bool readXML(const XmlElement* element) override;
    var writeJSON() const override;
    bool readJSON(const var& object) override;

    int load(const String& filename);
    [[nodiscard]] Mesh* getMesh(MeshLibrary& meshLibrary) const;


    /* ----------------------------------------------------------------------------- */

    int samplerate, fundNote, playbackPos;
    float phaseOffset;

    String uniqueName, lastLoadedFilePath;
    StereoBuffer audio;

    vector<PitchFrame> periods;
    int meshLayerIndex;

    Range<int>   midiLimits;
    Range<int>   midiRange;
    Range<float> veloRange;

private:
    JUCE_LEAK_DETECTOR(PitchedSample)
};

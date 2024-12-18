#pragma once

#include <set>
#include "../App/Doc/Savable.h"
#include "../App/SingletonAccessor.h"
#include "../Design/Updating/Updateable.h"

using std::set;

class Mesh;
class PitchedSample;
class MeshRasterizer;

class Multisample :
        public Savable
    ,	public SingletonAccessor
    ,	public Updateable {
public:
    struct NumberCode {
        NumberCode() : totalSize(0), average(0), posIndex(0) {}

        set<int> numbers;
        int totalSize, average, posIndex;
    };

    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void setMidiRange(const Range<int>& midiRange) = 0;
        virtual void setSampleMesh(Mesh* mesh) {}
        virtual void setWaveLoaded(bool loaded) {}
    };

    /* -------------------------------------------------------------------------- */

    Multisample(SingletonRepo* repo, MeshRasterizer* rasterizer);

    void clear();
    void createFromDirectory(const File& directory);
    void fillRanges();
    void parseRanges();
    void shiftAllByOctave(bool up);
    void updatePlaybackPosition();
    float getGreatestLengthSeconds();

    void performUpdate(UpdateType updateType) override;
    void writeXML(XmlElement* element) const override;
    bool readXML(const XmlElement* element) override;

    PitchedSample* addSample(const File& file, int defaultNote = -1);
    PitchedSample* getSampleForNote(int midiNote, float velocity);

    PitchedSample* getSampleAt(int index) {
        if (isPositiveAndBelow(index, samples.size()))
            return samples[index];

        return nullptr;
    }

    void setWaveRasterizer(MeshRasterizer* rasterizer) { waveRasterizer = rasterizer; }
    int size() 							{ return samples.size(); 	}
    PitchedSample* getCurrentSample() 	{ return current; 			}
    CriticalSection& getLock() 			{ return audioLock; 		}

private:
    void getModRanges(Range<int>& noteRange, Range<float>& velRange);

    MeshRasterizer* waveRasterizer;
    PitchedSample* current;
    CriticalSection audioLock;
    ListenerList<Listener> listeners;
    OwnedArray<PitchedSample, CriticalSection> samples;
};

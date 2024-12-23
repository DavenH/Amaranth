#include "Multisample.h"
#include "PitchedSample.h"
#include "../Design/Updating/Updater.h"
#include "../Algo/PitchTracker.h"
#include "../App/AppConstants.h"
#include "../App/SingletonRepo.h"
#include "../Util/Arithmetic.h"
#include "../Util/NumberUtils.h"
#include "../Util/Util.h"
#include "../Inter/MorphPositioner.h"
#include "../Definitions.h"

Multisample::Multisample(SingletonRepo* repo, MeshRasterizer* rasterizer) :
        SingletonAccessor(repo, "Multisample")
    ,	current(nullptr)
    ,	waveRasterizer(rasterizer) {
}

PitchedSample* Multisample::getSampleForNote(int midiNote, float velocity) {
    if (midiNote < 0 && velocity < 0)
        return samples.getFirst();

    NumberUtils::constrain(velocity, 0.00001f, 0.99999f);

    for (auto& sample : samples) {
        if(sample->midiRange.contains(midiNote) && sample->veloRange.contains(velocity))
            return sample;
        }

    return nullptr;
}

void Multisample::createFromDirectory(const File& directory) {
    ScopedLock sl(audioLock);

    Array<File> files;
    String exts[] = { "*.wav", "*.aif", "*.mp3", "*.ogg", "*.flac", "*.aiff" };

    for(const auto & ext : exts)
        directory.findChildFiles(files, File::findFiles, false, ext);

    samples.clear();

    for (auto& file : files) {
        std::unique_ptr<PitchedSample> sample(new PitchedSample());

        if (sample->load(file.getFullPathName()) >= 0) {
            samples.add(sample.release());
        }
    }

    parseRanges();
    fillRanges();
    performUpdate(Update);
}

void Multisample::fillRanges() {
    class SampleComparator {
    public:
        static int compareElements(const PitchedSample* first, const PitchedSample* second) {
            if (first->midiRange.getStart() < second->midiRange.getStart()) {
                return -1;
            }
            if (first->midiRange.getStart() == second->midiRange.getStart()) {
                if (first->veloRange.getEnd() < second->veloRange.getEnd()) {
                    return -1;
                }
                if (first->veloRange.getEnd() == second->veloRange.getEnd()) {
                    return 0;
                }
                return 1;
            }

            return 1;
        }
    };

    SampleComparator comparator;
    samples.sort(comparator);

    int j = 0;
    while(j < samples.size())
    {
        PitchedSample* sample = samples[j];
        int currentNote = sample->midiRange.getStart();

        sample->veloRange.setStart(0);

        vector<PitchedSample*> velocities;
        velocities.push_back(sample);


        // fill velocity ranges at current note
        ++j;
        while (j < samples.size() && samples[j]->midiRange.getStart() == currentNote) {
            velocities.push_back(samples[j]);
            samples[j]->veloRange.setStart(samples[j - 1]->veloRange.getEnd());
            ++j;
        }

        samples[j - 1]->veloRange.setEnd(1.f);

        // fill note range
        int nextNote = j < samples.size() ? samples[j]->midiRange.getStart() : getConstant(HighestMidiNote);

    for (auto& velocity : velocities)
        velocity->midiRange.setEnd(nextNote);
    }


    // extend lowest note to bottom
    if (samples.size() > 0) {
        int lowest = samples.getFirst()->midiRange.getStart();

        j = 0;
        while (j < samples.size() && samples[j]->midiRange.getStart() == lowest) {
            samples[j]->midiRange.setStart(getConstant(LowestMidiNote));
            ++j;
        }
    }
}

void Multisample::parseRanges() {
    int velocitiesFound = 0;
    int notesFound = 0;

    String common;
    if (samples.size() > 2) {
        String nameA = samples[0]->uniqueName;
        String nameB = samples[1]->uniqueName;

        StringArray tokensA, tokensB;
        tokensA.addTokens(nameA, " _");
        tokensB.addTokens(nameB, " _");

        for (auto& tokenA : tokensA) {
            for (auto &tokenB : tokensB) {
                if (tokenA == tokenB) {
                    if (tokenA.length() > common.length()) {
                        common = tokenA;
                    }
                }
            }
        }
    }

    for (auto sample : samples) {
        String name = File(sample->lastLoadedFilePath).getFileNameWithoutExtension();
        int index 	= name.indexOfWholeWord(common);

        if (index >= 0) {
            String composite = name.substring(0, index) + name.substring(index + common.length());
            sample->uniqueName = composite;
        } else {
            sample->uniqueName = name;
        }

        int midiNote = Util::extractPitchFromFilename	(sample->uniqueName, false);
        float veloc = Util::extractVelocityFromFilename(sample->uniqueName) / 127.f;

        if (veloc >= 0) {
            sample->veloRange = Range(veloc, veloc);
            ++velocitiesFound;
        }

        if (midiNote > 10) {
            midiNote += 12;
            sample->midiRange.setStart(midiNote);
            sample->fundNote = midiNote;

            ++notesFound;
        }
    }

    // try parsing velocity numbers out of filename
    if (velocitiesFound < samples.size() / 2 || notesFound < samples.size() / 2) {
        vector<NumberCode> codeSets;

        for (auto sample : samples) {
            String name = sample->lastLoadedFilePath;

            vector<int> ints = Util::getIntegersInString(sample->uniqueName);

            for (int j = 0; j < (int) ints.size(); ++j) {
                if (codeSets.size() <= j) {
                    NumberCode codeSet;
                    codeSet.posIndex = j;
                    codeSets.push_back(codeSet);
                }

                NumberCode& codeSet = codeSets[j];
                codeSet.numbers.insert(ints[j]);
                codeSet.totalSize++;
            }
        }

        int velPos  = -1;
        int notePos = -1;

        for (int i = 0; i < (int) codeSets.size(); ++i) {
            NumberCode& codeSet = codeSets[i];
            codeSet.average = 0;

            for(auto it = codeSet.numbers.begin(); it != codeSet.numbers.end(); ++it)
                codeSet.average += *it;

            codeSet.average /= codeSet.numbers.size();

            if(codeSet.numbers.size() > 5 && NumberUtils::within(codeSet.average, 40, 100)) {
                notePos = i;
            }

            if(codeSet.numbers.size() <= 5 && NumberUtils::within(codeSet.average, 60, 127)) {
                velPos = i;
            }
        }

        if (notePos >= 0 || velPos >= 0) {
            for (auto sample : samples) {
                vector<int> ints = Util::getIntegersInString(sample->uniqueName);

                if (isPositiveAndBelow(notePos, (int) ints.size())) {
                    int note = ints[notePos] + 12;

                    sample->midiRange.setStart(note);
                    sample->fundNote = note;
                }

                if(isPositiveAndBelow(velPos, (int) ints.size())) {
                    sample->veloRange.setEnd(ints[velPos] / 127.f);
                }
            }
        }
    }
}

void Multisample::getModRanges(Range<int>& noteRange, Range<float>& velRange) {
    MorphPosition m = repo->getMorphPosition().getOffsetPosition(true);

    Range range(getConstant(LowestMidiNote), getConstant(HighestMidiNote));
    float topVel = 1.f, bttmVel = 0.f;
    int bttmNote = range.getStart();
    int topNote = range.getEnd();

    if (m.redDepth < 1.f) {
        bttmNote = Arithmetic::getNoteForValue(m.red, range);
        topNote = Arithmetic::getNoteForValue(m.redEnd(), range);
    }

    if (m.blueDepth < 1.f) {
        bttmVel	= jmax(0.f, 1.f - m.blueEnd());
        topVel = jmin(1.f, 1.f - m.blue);
    }

    noteRange = Range(bttmNote, topNote);
    velRange = Range(bttmVel,  topVel);
}

void Multisample::clear() {
    listeners.call(&Listener::setMidiRange, Range<int>());

    ScopedLock slB(audioLock);

    current = nullptr;
    samples.clear();
}

PitchedSample* Multisample::addSample(const File& file, int defaultNote) {
    Range<int> noteRange;
    Range<float> velRange;

    getModRanges(noteRange, velRange);

    ScopedLock sl(audioLock);

    std::unique_ptr<PitchedSample> sample(new PitchedSample());

    sample->midiRange 	= noteRange;
    sample->veloRange 	= velRange;

    if(sample->load(file.getFullPathName()) >= 0) {
        if(defaultNote >= Constants::LowestMidiNote)
            sample->fundNote = defaultNote;

        current = sample.release();

        vector<PitchedSample*> toRemove;

        for (auto cand : samples) {
            if(sample->midiRange.intersects(cand->midiRange) && sample->veloRange.intersects(cand->veloRange))
                toRemove.push_back(cand);
        }

        for(auto& i : toRemove)
            samples.removeObject(i, true);

        samples.add(current);
        fillRanges();

        return current;
    }

    return nullptr;
}

float Multisample::getGreatestLengthSeconds() {
    float greatest = 0;

    ScopedLock sl(audioLock);

    for (auto sample : samples) {
        greatest = jmax(greatest, sample->size() / float(sample->samplerate));
    }

    return greatest;
}

void Multisample::writeXML(XmlElement* element) const {
    if (element == nullptr) {
        return;
    }

    for (auto sample : samples) {
        auto* sampleElem = new XmlElement("Sample");

        sample->writeXML(sampleElem);
        element->addChildElement(sampleElem);
    }
}

bool Multisample::readXML(const XmlElement* element) {
    ScopedLock sl(audioLock);

    // old presets can load one
    if (samples.size() > 1) {
        samples.clear();
    }

    for(auto sampleElem : element->getChildWithTagNameIterator("Sample")) {
        std::unique_ptr<PitchedSample> sample(new PitchedSample());
        sample->readXML(sampleElem);
        sample->createPeriodsFromEnv(waveRasterizer);

        samples.add(sample.release());
    }

    if (samples.size() > 0) {
        listeners.call(&Listener::setWaveLoaded, true);
        performUpdate(Update);
    }

    return true;
}

void Multisample::performUpdate(UpdateType updateType) {
    if (updateType == Update) {
        MorphPosition pos = repo->getMorphPosition().getMorphPosition();
        float midiNote = Arithmetic::getNoteForValue(pos.red, Range(getConstant(LowestMidiNote), getConstant(HighestMidiNote)));

        current = getSampleForNote(midiNote, 1 - pos.blue);

        Mesh* mesh = nullptr;

        if (current != nullptr) {
            mesh = current->mesh.get();
            listeners.call(&Listener::setMidiRange, current->midiRange);
        }

        listeners.call(&Listener::setSampleMesh, mesh);
    }
}

void Multisample::updatePlaybackPosition() {
    float progress 	= repo->getMorphPosition().getYellow();
    float seconds  	= getGreatestLengthSeconds();
    int newPosition = roundToInt(progress * seconds * 44100.f);

    for(auto sample : samples) {
        sample->playbackPos = newPosition;
    }
}

void Multisample::shiftAllByOctave(bool up) {
    for (auto sample : samples) {
        sample->shiftOctave(up);
        PitchTracker::refineFrames(sample, sample->getAveragePeriod());
        sample->createEnvFromPeriods(true);
        sample->createPeriodsFromEnv(waveRasterizer);
    }

    fillRanges();
}

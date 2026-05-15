#include <catch2/catch_test_macros.hpp>

#include <App/AppConstants.h>
#include <App/Doc/Document.h>
#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Audio/Multisample.h>
#include <Audio/PitchedSample.h>
#include <Inter/MorphPositioner.h>
#include <JuceHeader.h>
#include <Util/Arithmetic.h>

using namespace juce;

namespace {
    class TestMorphPositioner :
            public MorphPositioner {
    public:
        void setMidiRange(int startNote, int endNote) {
            Range<int> midiLimits(Constants::LowestMidiNote, Constants::HighestMidiNote);

            position.red.setValueDirect(Arithmetic::getUnitValueForNote(startNote, midiLimits));
            position.redDepth = Arithmetic::getUnitValueForNote(endNote, midiLimits) -
                                Arithmetic::getUnitValueForNote(startNote, midiLimits);
            position.blue.setValueDirect(0.f);
            position.blueDepth = 1.f;
            position.time.setValueDirect(0.f);
            position.timeDepth = 1.f;
        }

        MorphPosition getMorphPosition() override {
            return position;
        }

        MorphPosition getOffsetPosition(bool withDepths) override {
            ignoreUnused(withDepths);
            return position;
        }

    private:
        MorphPosition position;
    };

    class ScopedJuceGui {
    public:
        ScopedJuceGui() {
            if (refCount++ == 0) {
                juceInitialiser = std::make_unique<ScopedJuceInitialiser_GUI>();
            }
        }

        ~ScopedJuceGui() {
            if (--refCount == 0) {
                juceInitialiser = nullptr;
            }
        }

    private:
        inline static int refCount = 0;
        inline static std::unique_ptr<ScopedJuceInitialiser_GUI> juceInitialiser;
    };

    void seedDefaultMeshLibrary(SingletonRepo& repo) {
        auto& meshLib = repo.get<MeshLibrary>("MeshLibrary");

        meshLib.addGroup(MeshLibrary::TypeEnvelope);
        meshLib.addGroup(MeshLibrary::TypeEnvelope);
        meshLib.addGroup(MeshLibrary::TypeEnvelope);
        meshLib.addGroup(MeshLibrary::TypeMesh);
        meshLib.addGroup(MeshLibrary::TypeMesh);
        meshLib.addGroup(MeshLibrary::TypeMesh);
        meshLib.addGroup(MeshLibrary::TypeMesh);
        meshLib.addGroup(MeshLibrary::TypeEnvelope);
        meshLib.addGroup(MeshLibrary::TypeMesh);
        meshLib.addGroup(MeshLibrary::TypeMesh);
        meshLib.addGroup(MeshLibrary::TypeMesh);

        meshLib.addLayer(LayerGroups::GroupWavePitch);
    }

    File writeTestWaveFile() {
        File file = File::getSpecialLocation(File::tempDirectory)
                .getNonexistentChildFile("amaranth-multisample", ".wav");

        constexpr int sampleRate = 44100;
        constexpr int sampleCount = 4096;

        AudioBuffer<float> buffer(1, sampleCount);
        auto* data = buffer.getWritePointer(0);

        for (int i = 0; i < sampleCount; ++i) {
            data[i] = (i % 64) < 32 ? 0.4f : -0.4f;
        }

        WavAudioFormat format;
        std::unique_ptr<FileOutputStream> stream(file.createOutputStream());
        REQUIRE(stream != nullptr);

        std::unique_ptr<AudioFormatWriter> writer(
                format.createWriterFor(stream.release(), sampleRate, 1, 16, {}, 0));
        REQUIRE(writer != nullptr);
        REQUIRE(writer->writeFromAudioSampleBuffer(buffer, 0, sampleCount));

        return file;
    }
}

TEST_CASE("Multisample keeps sample mesh layers distinct when replacing overlaps", "[Multisample]") {
    ScopedJuceGui juceGui;
    SingletonRepo repo;
    TestMorphPositioner positioner;

    repo.add(new AppConstants(&repo));
    repo.add(new Document(&repo));
    repo.add(new EditWatcher(&repo));
    repo.add(new MeshLibrary(&repo));
    repo.setMorphPositioner(&positioner);
    seedDefaultMeshLibrary(repo);

    Multisample multisample(&repo, nullptr);
    File waveFile = writeTestWaveFile();

    positioner.setMidiRange(40, 50);
    PitchedSample* lowSample = multisample.addSample(waveFile, 60);
    REQUIRE(lowSample != nullptr);
    REQUIRE(lowSample->meshLayerIndex == 0);
    lowSample->midiRange = Range<int>(40, 50);

    positioner.setMidiRange(70, 80);
    PitchedSample* highSample = multisample.addSample(waveFile, 72);
    REQUIRE(highSample != nullptr);
    REQUIRE(highSample->meshLayerIndex == 1);
    highSample->midiRange = Range<int>(70, 80);

    positioner.setMidiRange(40, 50);
    PitchedSample* replacement = multisample.addSample(waveFile, 60);
    REQUIRE(replacement != nullptr);

    REQUIRE(multisample.size() == 2);
    REQUIRE(highSample->meshLayerIndex == 1);
    REQUIRE(replacement->meshLayerIndex == 0);

    auto& meshLibrary = repo.get<MeshLibrary>("MeshLibrary");
    REQUIRE(highSample->getMesh(meshLibrary) != nullptr);
    REQUIRE(replacement->getMesh(meshLibrary) != nullptr);
    REQUIRE(highSample->getMesh(meshLibrary) != replacement->getMesh(meshLibrary));

    waveFile.deleteFile();
}

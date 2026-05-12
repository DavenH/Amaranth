#include <catch2/catch_test_macros.hpp>

#include <string>

#include <App/Doc/Document.h>
#include <App/Doc/DocumentDetails.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Design/Updating/Updater.h>
#include <Curve/Curve.h>
#include <JuceHeader.h>

#include "../Initializer.h"
#include <Inter/Interactor.h>
#include "../../Util/CycleEnums.h"
#include "../../UI/Effects/EffectGuiRegistry.h"
#include "../../UI/Effects/IrModellerUI.h"
#include "../../UI/Effects/WaveshaperUI.h"
#include "../../UI/Panels/MainPanel.h"
#include "../../UI/Panels/ModMatrixPanel.h"
#include "../../UI/Panels/OscControlPanel.h"
#include "../../UI/Panels/Morphing/MorphPanel.h"
#include "../../UI/VertexPanels/Envelope2D.h"
#include "../../UI/VertexPanels/GuideCurvePanel.h"

using namespace juce;

namespace {
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

        meshLib.addLayer(LayerGroups::GroupVolume);
        meshLib.addLayer(LayerGroups::GroupPitch);
        meshLib.addLayer(LayerGroups::GroupScratch);
        meshLib.addLayer(LayerGroups::GroupGuideCurve);
        meshLib.addLayer(LayerGroups::GroupTime);
        meshLib.addLayer(LayerGroups::GroupSpect);
        meshLib.addLayer(LayerGroups::GroupPhase);
        meshLib.addLayer(LayerGroups::GroupWavePitch);
        meshLib.addLayer(LayerGroups::GroupWaveshaper);
        meshLib.addLayer(LayerGroups::GroupIrModeller);

        Mesh* waveshaperMesh = meshLib.getCurrentMesh(LayerGroups::GroupWaveshaper);
        Mesh* irModellerMesh = meshLib.getCurrentMesh(LayerGroups::GroupIrModeller);

        repo.get<WaveshaperUI>("WaveshaperUI").getEffectRasterizer()->setMesh(waveshaperMesh);
        repo.get<IrModellerUI>("IrModellerUI").getEffectRasterizer()->setMesh(irModellerMesh);
        repo.get<IrModeller>("IrModeller").setMesh(irModellerMesh);
    }

    class ScopedPresetLoadSuppression {
    public:
        explicit ScopedPresetLoadSuppression(SingletonRepo& repo) :
                updater                     (repo.get<Updater>("Updater"))
            ,   ignoringEditMessages        (repo.get<Settings>("Settings").getGlobalSetting(AppSettings::IgnoringEditMessages), true)
            ,   ignoringMessages            (repo.get<Settings>("Settings").getGlobalSetting(AppSettings::IgnoringMessages), true) {
            updater.clearPendingUpdates();
        }

        ~ScopedPresetLoadSuppression() {
            updater.clearPendingUpdates();
        }

    private:
        Updater& updater;
        ScopedValueSetter<int> ignoringEditMessages;
        ScopedValueSetter<int> ignoringMessages;
    };

    class CycleTestHarness {
    public:
        CycleTestHarness() {
            if (juceInitRefCount++ == 0) {
                juceInitialiser = std::make_unique<ScopedJuceInitialiser_GUI>();
            }

            if (refCount++ == 0) {
                Curve::calcTable();
            }

            initializer = std::make_unique<Initializer>();
            repo = initializer->getSingletonRepo();

            repo->setSuppressAudioDeviceInit(true);
            repo->setSuppressSavableAutoRegistration(true);
            repo->setSuppressInitializerInit(true);
            repo->instantiate();
            initializer->setConstants();
            initializer->setDefaultSettings();
            initializer->instantiate();
            repo->setMorphPositioner(&repo->get<MorphPanel>("MorphPanel"));
            seedDefaultMeshLibrary(*repo);
            repo->init();
            initializer->doPostInitWiring();

            auto& document = repo->get<Document>("Document");
            document.registerSavable(&repo->get<MeshLibrary>("MeshLibrary"));
            document.registerSavable(&repo->get<OscControlPanel>("OscControlPanel"));
            document.registerSavable(&repo->get<Settings>("Settings"));
            document.registerSavable(&repo->get<MainPanel>("MainPanel"));
            document.registerSavable(&repo->get<ModMatrixPanel>("ModMatrixPanel"));
            document.registerSavable(&repo->get<GuideCurvePanel>("GuideCurvePanel"));
            document.registerSavable(&repo->get<MorphPanel>("MorphPanel"));
            document.registerSavable(&repo->get<Envelope2D>("Envelope2D"));
        }

        ~CycleTestHarness() {
            if (initializer != nullptr) {
                initializer->freeUIResources();
            }

            initializer = nullptr;

            if (--refCount == 0) {
                Curve::deleteTable();
            }

            if (--juceInitRefCount == 0) {
                juceInitialiser = nullptr;
            }
        }

        SingletonRepo& getRepo() const {
            return *repo;
        }

    private:
        inline static int refCount = 0;
        inline static int juceInitRefCount = 0;
        inline static std::unique_ptr<ScopedJuceInitialiser_GUI> juceInitialiser;

        std::unique_ptr<Initializer> initializer;
        SingletonRepo* repo{};
    };

    bool isLegacyXmlPreset(const File& presetFile) {
        std::unique_ptr<InputStream> stream(presetFile.createInputStream());
        DocumentDetails details;

        if (stream == nullptr || !Document::readHeader(stream.get(), details, 0xc0dedbad)) {
            return false;
        }

        stream->setPosition(Document::headerSizeBytes);
        GZIPDecompressorInputStream decompressor(stream.get(), false);
        String payload = decompressor.readEntireStreamAsString().trimStart();
        return payload.startsWithChar('<');
    }

    String normalisedPresetJson(Document& document) {
      #ifdef JUCE_DEBUG
        var parsed = JSON::parse(document.getPresetString());
        REQUIRE_FALSE(parsed.isVoid());

        auto* root = parsed.getDynamicObject();
        REQUIRE(root != nullptr);

        var preset = root->getProperty("preset");
        auto* presetObject = preset.getDynamicObject();
        REQUIRE(presetObject != nullptr);

        var details = presetObject->getProperty("details");
        auto* detailsObject = details.getDynamicObject();
        REQUIRE(detailsObject != nullptr);

        detailsObject->setProperty("modifiedMillis", 0);
        presetObject->removeProperty("modMatrix");

        return JSON::toString(parsed, true);
      #else
        ignoreUnused(document);
        FAIL("Round-trip test requires JUCE_DEBUG getPresetString()");
      #endif
    }
}

TEST_CASE("Legacy presets round trip through Document into stable current JSON", "[cycle][preset][roundtrip]") {
    File presetDir(String(CYCLE_SOURCE_DIR) + "/content/presets");
    Array<File> legacyPresets;

    REQUIRE(presetDir.isDirectory());
    presetDir.findChildFiles(legacyPresets, File::findFiles, false, "*.cyc");
    legacyPresets.sort();

    int exercisedPresetCount = 0;

    for (const auto& presetFile : legacyPresets) {
        if (!isLegacyXmlPreset(presetFile)) {
            continue;
        }

        DYNAMIC_SECTION("Round trip " << presetFile.getFileName().toStdString()) {
            CycleTestHarness harness;
            auto& repo = harness.getRepo();
            auto& document = repo.get<Document>("Document");
            std::string firstSnapshot;

            {
                ScopedPresetLoadSuppression suppressPresetUpdates(repo);
                REQUIRE(document.open(presetFile.getFullPathName()));
                document.getDetails().setFilename(presetFile.getFullPathName());
                firstSnapshot = normalisedPresetJson(document).toStdString();
            }

            MemoryOutputStream savedPreset;
            document.save(&savedPreset);
            REQUIRE(savedPreset.getDataSize() > Document::headerSizeBytes);

            MemoryInputStream savedInput(savedPreset.getData(), savedPreset.getDataSize(), false);

            std::string secondSnapshot;
            {
                ScopedPresetLoadSuppression suppressPresetUpdates(repo);
                REQUIRE(document.open(&savedInput));
                secondSnapshot = normalisedPresetJson(document).toStdString();
            }

            const bool snapshotsMatch = secondSnapshot == firstSnapshot;
            REQUIRE(snapshotsMatch);

            ++exercisedPresetCount;
        }

        break;
    }

    REQUIRE(exercisedPresetCount > 0);
}

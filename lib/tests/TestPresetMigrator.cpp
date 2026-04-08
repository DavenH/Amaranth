#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/App/Doc/DocumentDetails.h"
#include "../src/App/MeshLibrary.h"
#include "../src/App/Doc/PresetJson.h"
#include "../src/App/Doc/PresetMigrator.h"

using Catch::Approx;
using namespace juce;

namespace {
    var property(const var& object, const Identifier& name) {
        return PresetJson::property(object, name);
    }

    const Array<var>& requireArray(const var& value) {
        auto* array = PresetJson::getArray(value);
        REQUIRE(array != nullptr);
        return *array;
    }

    const DynamicObject* requireObject(const var& value) {
        auto* object = PresetJson::getObject(value);
        REQUIRE(object != nullptr);
        return object;
    }

    std::unique_ptr<XmlElement> parseXml(const String& xml) {
        XmlDocument document(xml);
        return std::unique_ptr<XmlElement>(document.getDocumentElement());
    }
}

TEST_CASE("PresetMigrator migrates current XML sections into V2 JSON", "[preset][migration]") {
    auto presetXml = parseXml(R"xml(
<Preset>
  <MeshLibrary>
    <Group mesh-type="0">
      <Layer active="1" gain="0.75" range="1.25" mode="2" scratch-chan="3" fine-tune="0.125" pan="-0.25">
        <Mesh>
          <Vertex time="0.1" phase="0.2" amp="0.3" key="0.4" mod="0.5" weight="0.6" id="7"/>
        </Mesh>
      </Layer>
    </Group>
  </MeshLibrary>
  <OscControls>
    <Knobs>
      <Knob number="0" value="0.2"/>
      <Knob number="2" value="0.8"/>
    </Knobs>
  </OscControls>
  <GuideCurveProps>
    <Properties noiseLevel="0.3" offsetLevel="0.4" phaseLevel="0.5" noiseSeed="19"/>
  </GuideCurveProps>
  <EnvelopeProps currentEnvGroup="2" volumeCurrentIndex="0" pitchCurrentIndex="0" scratchCurrentIndex="0" wavePitchCurrentIndex="0">
    <VolumeProps active="1" gain="0.5" range="0.6" mode="0" scratch-chan="-1" fine-tune="0.0" pan="0.1" dynamic="1" tempo-sync="1" global="0" logarithmic="1" scale="2">
      <EnvelopeMesh name="VolEnv">
        <MainMesh>
          <Mesh name="VolEnvMain" version="2">
            <Vertex time="0.12" phase="0.22" amp="0.32" key="0.42" mod="0.52" weight="0.62" id="9"/>
          </Mesh>
        </MainMesh>
      </EnvelopeMesh>
    </VolumeProps>
  </EnvelopeProps>
  <MorphPanel time="0.15" red="0.25" blue="0.35" pan="0.45"
              timeViewDepth="0.55" redViewDepth="0.65" blueViewDepth="0.75"
              timeInsertDepth="0.85" redInsertDepth="0.95" blueInsertDepth="1.05"
              currentMorphAxis="1"
              linkYellow="1" linkRed="0" linkBlue="1"
              useYellowDepth="1" useRedDepth="0" useBlueDepth="1"/>
  <ModMatrix>
    <Inputs><input id="3"/></Inputs>
    <Outputs><output id="8"/></Outputs>
    <Mappings><mapping in="3" out="8" dim="1"/></Mappings>
  </ModMatrix>
</Preset>
)xml");

    REQUIRE(presetXml != nullptr);

    DocumentDetails details;
    details.setName("Migrated Current");
    details.setAuthor("Test");

    var root = PresetMigrator::migrateXmlToCurrentJson(presetXml.get(), details);
    REQUIRE(property(root, "format").toString() == "amaranth-preset");
    REQUIRE(int(property(root, "schemaVersion")) == 2);

    var preset = property(root, "preset");
    requireObject(preset);
    REQUIRE(property(property(preset, "details"), "name").toString() == "Migrated Current");

    const auto& groups = requireArray(property(property(preset, "meshLibrary"), "groups"));
    REQUIRE(groups.size() == 1);

    const auto& layers = requireArray(property(groups.getReference(0), "layers"));
    REQUIRE(layers.size() == 1);
    REQUIRE(double(property(property(layers.getReference(0), "properties"), "gain")) == Approx(0.75));

    const auto& vertices = requireArray(property(property(layers.getReference(0), "mesh"), "vertices"));
    REQUIRE(vertices.size() == 1);
    REQUIRE(int(property(vertices.getReference(0), "id")) == 7);

    const auto& knobs = requireArray(property(property(preset, "oscControls"), "knobs"));
    REQUIRE(knobs.size() == 3);
    REQUIRE(double(knobs.getReference(1)) == Approx(0.0));
    REQUIRE(double(knobs.getReference(2)) == Approx(0.8));

    const auto& guides = requireArray(property(property(preset, "guideCurveProps"), "guides"));
    REQUIRE(guides.size() == 1);
    REQUIRE(int(property(guides.getReference(0), "noiseSeed")) == 19);

    REQUIRE(int(property(property(preset, "envelopeProps"), "currentGroup")) == 2);
    auto envelopeGroups = property(property(preset, "envelopeProps"), "groups");
    const auto& volumeEnvLayers = requireArray(property(property(envelopeGroups, "volume"), "layers"));
    REQUIRE(volumeEnvLayers.size() == 1);
    REQUIRE(property(property(volumeEnvLayers.getReference(0), "mesh"), "name").toString() == "VolEnv");
    REQUIRE(bool(property(property(volumeEnvLayers.getReference(0), "properties"), "tempoSync")));

    var morphPanel = property(preset, "morphPanel");
    REQUIRE(double(property(property(morphPanel, "position"), "time")) == Approx(0.15));
    REQUIRE(int(property(morphPanel, "primaryAxis")) == 1);
    REQUIRE(bool(property(property(morphPanel, "linking"), "time")));
    REQUIRE(bool(property(property(morphPanel, "rangeEnabled"), "blue")));

    const auto& mappings = requireArray(property(property(preset, "modMatrix"), "mappings"));
    REQUIRE(mappings.size() == 1);
    REQUIRE(int(property(mappings.getReference(0), "dim")) == 1);
}

TEST_CASE("PresetMigrator remaps legacy V1 XML sections into current mesh groups", "[preset][migration]") {
    auto presetXml = parseXml(R"xml(
<Preset>
  <AllMeshes>
    <TimeLayer>
      <TimeMesh>
        <Mesh name="TimeMesh" version="2">
          <Vertex time="0.11" phase="0.22" amp="0.33" key="0.44" mod="0.55" weight="0.66" id="12"/>
        </Mesh>
      </TimeMesh>
    </TimeLayer>
    <WaveShaperLayer>
      <ShaperMesh>
        <Mesh name="WaveShaperMesh" version="1">
          <Vertex time="0.9" phase="0.8" amp="0.7" key="0.6" mod="0.5" weight="0.4" id="2"/>
        </Mesh>
      </ShaperMesh>
    </WaveShaperLayer>
    <EnvLayer>
      <EnvVolumeMesh>
        <EnvelopeMesh name="VolEnv" primaryEnabled="0" sustainIndex="2" sustainIndex2="5">
          <MainMesh>
            <Mesh name="EnvMain" version="3">
              <Vertex time="0.15" phase="0.25" amp="0.35" key="0.45" mod="0.55" weight="0.65" id="21"/>
            </Mesh>
          </MainMesh>
        </EnvelopeMesh>
      </EnvVolumeMesh>
    </EnvLayer>
  </AllMeshes>
  <TimeDomainProperties>
    <TimeProperties isEnabled="0" pan="0.125" fine="0.75" dynamicRange="0.9"/>
  </TimeDomainProperties>
  <DeformerProps>
    <Properties noiseLevel="0.7" offsetLevel="0.8" phaseLevel="0.9" noiseSeed="41"/>
  </DeformerProps>
  <ModMapping modMappingId="6"/>
</Preset>
)xml");

    REQUIRE(presetXml != nullptr);

    DocumentDetails details;
    details.setName("Migrated Legacy");

    var root = PresetMigrator::migrateV1XmlToCurrentJson(presetXml.get(), details);
    var preset = property(root, "preset");
    const auto& groups = requireArray(property(property(preset, "meshLibrary"), "groups"));

    REQUIRE(groups.size() >= 10);

    const auto& timeLayers = requireArray(property(groups.getReference(4), "layers"));
    REQUIRE(timeLayers.size() == 1);
    REQUIRE(property(property(timeLayers.getReference(0), "mesh"), "name").toString() == "TimeMesh");
    REQUIRE_FALSE(bool(property(property(timeLayers.getReference(0), "properties"), "active")));
    REQUIRE(double(property(property(timeLayers.getReference(0), "properties"), "pan")) == Approx(0.125));
    REQUIRE(double(property(property(timeLayers.getReference(0), "properties"), "fineTune")) == Approx(0.75));
    REQUIRE(double(property(property(timeLayers.getReference(0), "properties"), "range")) == Approx(0.9));

    const auto& volumeLayers = requireArray(property(groups.getReference(0), "layers"));
    REQUIRE(int(property(groups.getReference(0), "meshType")) == MeshLibrary::TypeEnvelope);
    REQUIRE(volumeLayers.size() == 1);
    REQUIRE_FALSE(bool(property(property(volumeLayers.getReference(0), "properties"), "active")));
    REQUIRE(property(property(volumeLayers.getReference(0), "mesh"), "name").toString() == "VolEnv");

    const auto& loopIndices = requireArray(property(property(volumeLayers.getReference(0), "mesh"), "loopIndices"));
    const auto& sustainIndices = requireArray(property(property(volumeLayers.getReference(0), "mesh"), "sustainIndices"));
    REQUIRE(loopIndices.size() == 1);
    REQUIRE(int(loopIndices.getReference(0)) == 2);
    REQUIRE(sustainIndices.size() == 1);
    REQUIRE(int(sustainIndices.getReference(0)) == 5);

    const auto& waveshaperLayers = requireArray(property(groups.getReference(9), "layers"));
    REQUIRE(waveshaperLayers.size() == 1);
    REQUIRE(property(property(waveshaperLayers.getReference(0), "mesh"), "name").toString() == "WaveShaperMesh");

    const auto& guides = requireArray(property(property(preset, "guideCurveProps"), "guides"));
    REQUIRE(guides.size() == 1);
    REQUIRE(int(property(guides.getReference(0), "noiseSeed")) == 41);

    REQUIRE(int(property(property(preset, "morphPanel"), "modMappingId")) == 6);
}

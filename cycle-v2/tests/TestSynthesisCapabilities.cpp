#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/CycleDelay.h>
#include <Audio/CycleDsp/EffectParameterMapping.h>
#include <Audio/CycleDsp/IrModel.h>
#include <Audio/CycleDsp/SpectralLayerCore.h>
#include <Util/LogRegionMapping.h>

#include <limits>

#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Graph/SynthesisCapabilities.h"

using namespace CycleV2;

namespace {

var property(const var& value, const Identifier& name) {
    REQUIRE(value.getDynamicObject() != nullptr);
    return value.getDynamicObject()->getProperty(name);
}

var find(const var& values, const String& key, const String& id) {
    REQUIRE(values.isArray());
    for (const auto& value : *values.getArray()) {
        if (property(value, Identifier(key)).toString() == id) {
            return value;
        }
    }
    FAIL("Missing exported record: " << id);
    return {};
}

var convert(const char* type, const char* parameter, double value, bool inverse = false,
        ParameterMappingContext context = {}) {
    String error;
    auto result = SynthesisCapabilities::convertParameter(type, parameter, value, inverse, context, error);
    REQUIRE(error.isEmpty());
    return result;
}

}

TEST_CASE("Capability export supplies codec-valid defaults and graph-valid parameter edits",
        "[cycle-v2][capabilities]") {
    const auto exported = SynthesisCapabilities::exportJSON();
    const auto nodes = property(exported, "nodes");
    REQUIRE(nodes.size() == (int) NodeDefinitionRegistry::instance().definitions().size());
    for (const auto& node : *nodes.getArray()) {
        const auto* definition = NodeDefinitionRegistry::instance().find(property(node, "typeId").toString());
        REQUIRE(definition != nullptr);
        NodeGraph graph;
        graph.addNode(GraphNodeFactory().createNode(definition->kind, "candidate", {}));
        const auto parameters = property(node, "parameters");
        for (const auto& parameter : *parameters.getArray()) {
            const String id = property(parameter, "id").toString();
            const String value = property(parameter, "defaultValue").toString();
            REQUIRE(GraphNodeStateEditor().setNodeParameter(
                    graph, "candidate", id, property(parameter, "label").toString(), value).succeeded());
            const auto minimum = property(parameter, "minimum");
            if (!minimum.isVoid()) {
                REQUIRE_FALSE(GraphNodeStateEditor().setNodeParameter(
                        graph, "candidate", id, id, String((double) minimum - 1.0)).succeeded());
            }
            const auto choices = property(parameter, "choices");
            if (choices.size() > 0) {
                REQUIRE_FALSE(GraphNodeStateEditor().setNodeParameter(
                        graph, "candidate", id, id, "not-a-valid-choice").succeeded());
            }
        }
        if (definition->modelCodec != nullptr) {
            String error;
            const auto model = property(node, "model");
            auto decoded = definition->modelCodec->readJSON(property(model, "default"), error);
            REQUIRE(error.isEmpty());
            REQUIRE(decoded != nullptr);
            REQUIRE(decoded->schemaId() == property(model, "schemaId").toString());
            REQUIRE(decoded->schemaVersion() == (int) property(model, "version"));
        }
    }
}

TEST_CASE("Physical conversion discovery agrees with canonical effect mappings and authoring normalization",
        "[cycle-v2][capabilities]") {
    const ParameterMappingContext context { 96000.0, 7 };
    for (const double value : { 0.0, 0.13, 0.5, 0.79, 1.0 }) {
        REQUIRE((double) property(convert("equalizer", "band2Frequency", value), "physicalValue")
                == Catch::Approx(CycleDsp::equalizerFrequency((float) value)));
        REQUIRE((double) property(convert("trilinearMesh", "gain", value), "physicalValue")
                == Catch::Approx(CycleDsp::outputGain((float) value)));
        REQUIRE((double) property(convert("delay", "spinIters", value), "physicalValue")
                == CycleDsp::delaySpinIterations(value));
        REQUIRE((double) property(convert("waveshaper", "post", value), "physicalValue")
                == Catch::Approx(CycleDsp::waveshaperGainDecibels((float) value)));
        REQUIRE((double) property(convert("delay", "time", value, false, context), "physicalValue")
                == Catch::Approx(CycleDsp::delayBeats((float) value, 7)));
        REQUIRE((double) property(convert("reverb", "size", value, false, context), "physicalValue")
                == Catch::Approx(CycleDsp::reverbKernelSeconds((float) value, 96000.0)));
    }
    const auto frequency = convert("equalizer", "band3Frequency", 3500.0, true);
    REQUIRE(property(frequency, "storedValue").toString().getDoubleValue()
            == Catch::Approx(CycleDsp::equalizerFrequencyUnitValue(3500.f)));
    REQUIRE((double) property(frequency, "physicalValue") == Catch::Approx(3500).epsilon(0.0001));

    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(graph, "ir", "size", "Size", "0.51").succeeded());
    const auto size = convert("impulseResponse", "size", 0.51);
    REQUIRE(property(size, "storedValue").toString()
            == parameterValueForNode(*graph.findNode("ir"), "size"));
    REQUIRE((double) property(size, "physicalValue") == CycleDsp::irImpulseLength(0.51));
    REQUIRE((double) property(convert("impulseResponse", "post", 0.8), "physicalValue")
            == Catch::Approx(CycleDsp::irPostGainDecibels(0.8)));
    REQUIRE((double) property(convert("impulseResponse", "highPass", 0.3, false, context), "physicalValue")
            == Catch::Approx(CycleDsp::irPrefilterFrequency(0.3, context.sampleRate)));
}

TEST_CASE("Conversion rejects unavailable mappings and invalid domains instead of silently clamping",
        "[cycle-v2][capabilities]") {
    const auto rejected = [](const String& type, const String& parameter, double value,
            bool inverse = false, ParameterMappingContext context = {}) {
        String error;
        REQUIRE(SynthesisCapabilities::convertParameter(
                type, parameter, value, inverse, context, error).isVoid());
        REQUIRE(error.isNotEmpty());
    };
    rejected("missing", "gain", 0.5);
    rejected("trilinearMesh", "range", 0.5);
    rejected("equalizer", "band1Frequency", -0.1);
    rejected("equalizer", "band1Frequency", -1.0e-13);
    rejected("equalizer", "band1Frequency", 20000.0, true);
    rejected("reverb", "size", 1.0, true);
    rejected("delay", "spinIters", 2.5, true);
    rejected("impulseResponse", "size", 4096.5, true);
    rejected("output", "gain", std::numeric_limits<double>::quiet_NaN());
    rejected("delay", "time", 0.5, false, { 44100.0, 0 });
    rejected("reverb", "size", 0.5, false, { -1.0, 4 });
    REQUIRE(SynthesisCapabilities::exportJSON({ -1.0, 4 }).isVoid());
}

TEST_CASE("Discovery distinguishes destination control timing and scoped effect contracts",
        "[cycle-v2][capabilities]") {
    const auto nodes = property(SynthesisCapabilities::exportJSON(), "nodes");
    const auto mesh = find(nodes, "typeId", "trilinearMesh");
    const auto envelope = find(nodes, "typeId", "envelope");
    REQUIRE(property(find(property(mesh, "inputs"), "id", "red"), "controlTiming").toString()
            == "continuous");
    REQUIRE(property(find(property(envelope, "inputs"), "id", "red"), "controlTiming").toString()
            == "noteOnLatched");
    REQUIRE(property(find(property(mesh, "inputs"), "id", "context"), "controlTiming").toString()
            == "unspecified");
    REQUIRE(property(find(nodes, "typeId", "reverb"), "processingCapability").toString() == "globalOnly");
    REQUIRE(property(find(nodes, "typeId", "equalizer"), "processingCapability").toString() == "selectable");
}

TEST_CASE("Spectral authoring discovery preserves runtime harmonic coordinates and operand scaling",
        "[cycle-v2][capabilities][spectral-authoring]") {
    const std::vector<NodeParameter> parameters {
        { "signalType", {}, "spectralMagnitude" },
        { "range", {}, "0.333333333333" },
        { "gain", {}, "0.5" }
    };
    String error;
    const auto authoring = SynthesisCapabilities::spectralAuthoring(parameters, 62, 2048, error);
    REQUIRE(error.isEmpty());
    const LogRegionMapping mapping(62 + LogRegionMapping::legacyMidiNoteBias);
    REQUIRE((int) property(authoring, "harmonicCount") == mapping.regionSize());
    REQUIRE((int) property(authoring, "harmonicCount") == 76);
    REQUIRE((double) property(authoring, "mappingSampleRate") == 44100.0);
    REQUIRE((double) property(authoring, "frequencyHz")
            == Catch::Approx(MidiMessage::getMidiNoteInHertz(62)));
    REQUIRE(property(authoring, "requiredPolarity").toString() == "unipolar");
    REQUIRE(property(authoring, "requiredPhaseMode").toString() == "absolute");
    REQUIRE(property(authoring, "fftNormalization").toString() == "DivFwdByN");
    REQUIRE((double) property(authoring, "ordinaryHarmonicPeakToMagnitude") == 0.5);
    REQUIRE((double) property(authoring, "dcMagnitude") == 0.0);
    REQUIRE((double) property(authoring, "nyquistPeakToMagnitude") == 1.0);
    ScopedAlloc<float> positions(mapping.regionSize());
    mapping.fillDisplayUnits(positions);
    const auto exportedPositions = property(authoring, "positions");
    REQUIRE(exportedPositions.size() == positions.size());
    for (int index = 0; index < positions.size(); ++index) {
        REQUIRE((double) exportedPositions[index] == positions[index]);
    }

    const float scale = (float) (double) property(authoring, "additiveUnitScale");
    const float exponent = (float) (double) property(authoring, "additiveExponent");
    REQUIRE(exponent == Catch::Approx(1.f));
    for (const float target : { 0.001f, 0.01f, 0.03f }) {
        float authoredAmplitude = std::pow(target / scale, 1.f / exponent);
        CycleDsp::SpectralLayerCore::shapeMagnitudeOperand(
                Buffer<float>(&authoredAmplitude, 1), 1.f / 3.f, true, false, mapping.regionSize());
        REQUIRE(authoredAmplitude == Catch::Approx(target).epsilon(1.0e-5f));
    }
    ScopedAlloc<float> phaseScales(mapping.regionSize());
    CycleDsp::SpectralLayerCore::preparePhaseHarmonicScale(phaseScales);
    phaseScales.mul(CycleDsp::SpectralLayerCore::phaseOffsetScale(1.f / 3.f)
            * MathConstants<float>::twoPi);
    const auto exportedPhases = property(authoring, "phaseScales");
    for (int index = 0; index < phaseScales.size(); ++index) {
        REQUIRE((double) exportedPhases[index] == Catch::Approx(phaseScales[index]));
    }

    const auto capped = SynthesisCapabilities::spectralAuthoring(parameters, 62, 64, error);
    REQUIRE(error.isEmpty());
    REQUIRE((int) property(capped, "regionSize") == 76);
    REQUIRE((int) property(capped, "harmonicCount") == 32);
    REQUIRE(property(capped, "positions").size() == 32);
    REQUIRE(property(capped, "phaseScales").size() == 32);
    float cappedScale = 1.f;
    CycleDsp::SpectralLayerCore::shapeMagnitudeOperand(
            Buffer<float>(&cappedScale, 1), 1.f / 3.f, true, false, 32);
    REQUIRE((double) property(capped, "additiveUnitScale") == Catch::Approx(cappedScale));

    for (const int note : { -1, 128 }) {
        REQUIRE(SynthesisCapabilities::spectralAuthoring(parameters, note, 2048, error).isVoid());
        REQUIRE(error.isNotEmpty());
    }
    REQUIRE(SynthesisCapabilities::spectralAuthoring(parameters, 62, 63, error).isVoid());
    REQUIRE(error.isNotEmpty());
    REQUIRE(SynthesisCapabilities::spectralAuthoring({}, 62, 2048, error).isVoid());
    REQUIRE(error.isNotEmpty());
}

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/CycleDelay.h>
#include <Audio/CycleDsp/EffectParameterMapping.h>
#include <Audio/CycleDsp/IrModel.h>

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

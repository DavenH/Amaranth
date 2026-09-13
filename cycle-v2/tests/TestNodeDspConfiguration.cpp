#include <array>

#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/EffectParameterMapping.h>

#include "Graph/GraphNodeFactory.h"
#include "Nodes/Effects/EffectSignalProcessors.h"
#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Runtime/NodeDspConfiguration.h"

using namespace CycleV2;

namespace {

class TestConfiguration final : public INodeDspConfiguration {
public:
    explicit TestConfiguration(int valueToUse) : value(valueToUse) {}

    AudioModuleRole role() const override { return AudioModuleRole::Waveshaper; }

    int value {};
};

void setParameter(Node& node, const String& id, const String& value) {
    for (auto& parameter : node.parameters) {
        if (parameter.id == id) {
            parameter.value = value;
            return;
        }
    }
    FAIL("Missing node parameter: " << id);
}

}

TEST_CASE("DSP configuration publication retains stable keys and complete revisions", "[cycle-v2][runtime][configuration]") {
    NodeConfigurationPublisher publisher;
    int buildCount = 0;

    const auto build = [&]() {
        ++buildCount;
        return std::make_shared<const TestConfiguration>(buildCount);
    };

    const auto first = publisher.publish("waveshaper:a", build);
    const auto unchanged = publisher.publish("waveshaper:a", build);
    const auto changed = publisher.publish("waveshaper:b", build);

    REQUIRE(first.revision == 1);
    REQUIRE(unchanged.revision == first.revision);
    REQUIRE(unchanged.value == first.value);
    REQUIRE(changed.revision == 2);
    REQUIRE(changed.value != first.value);
    REQUIRE(buildCount == 2);
}

TEST_CASE("Failed DSP configuration construction preserves the last valid revision", "[cycle-v2][runtime][configuration]") {
    NodeConfigurationPublisher publisher;
    const auto valid = publisher.publish("valid", []() {
        return std::make_shared<const TestConfiguration>(7);
    });
    const auto failed = publisher.publish("invalid", []() {
        return std::shared_ptr<const INodeDspConfiguration> {};
    });

    REQUIRE(failed.revision == valid.revision);
    REQUIRE(failed.key == valid.key);
    REQUIRE(failed.value == valid.value);
}

TEST_CASE("Published DSP configurations outlive publisher replacement", "[cycle-v2][runtime][configuration]") {
    NodeConfigurationPublisher publisher;
    const auto first = publisher.publish("first", []() {
        return std::make_shared<const TestConfiguration>(11);
    });
    std::weak_ptr<const INodeDspConfiguration> oldLifetime = first.value;

    publisher.publish("second", []() {
        return std::make_shared<const TestConfiguration>(12);
    });

    REQUIRE_FALSE(oldLifetime.expired());
    REQUIRE(std::static_pointer_cast<const TestConfiguration>(first.value)->value == 11);
}

TEST_CASE("Reverb factory carries immutable kernels across mix-only publications",
        "[cycle-v2][runtime][configuration][reverb]") {
    Node reverb = GraphNodeFactory().createNode(NodeKind::Reverb, "reverb", {});
    NodeDspConfigurationFactory factory;
    const auto first = std::dynamic_pointer_cast<const ReverbConfiguration>(
            factory.create(AudioModuleRole::Reverb, reverb.parameters, reverb.model, {}));
    REQUIRE(first != nullptr);

    setParameter(reverb, "wet", "0.8");
    const auto wetEdit = std::dynamic_pointer_cast<const ReverbConfiguration>(
            factory.create(
                    AudioModuleRole::Reverb,
                    reverb.parameters,
                    reverb.model,
                    {},
                    nullptr,
                    {},
                    {},
                    first.get()));

    REQUIRE(wetEdit != nullptr);
    REQUIRE(wetEdit->kernel == first->kernel);
    REQUIRE(wetEdit->wetLevel != first->wetLevel);
}

TEST_CASE("Time Trimesh configuration applies its authored output gain",
        "[cycle-v2][runtime][configuration][trimesh]") {
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    setParameter(node, "gain", "0.75");

    const auto configuration = std::dynamic_pointer_cast<const TrimeshConfiguration>(
            NodeDspConfigurationFactory().create(
                    AudioModuleRole::MeshSource,
                    node.parameters,
                    node.model,
                    {}));

    REQUIRE(configuration != nullptr);
    REQUIRE(configuration->gain == CycleDsp::outputGain(0.75f));
}

TEST_CASE("Direct spectral Trimesh applies its range before IFFT",
        "[cycle-v2][runtime][configuration][spectral]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    Node magnitudeNode = factory.createNode(NodeKind::TrilinearMesh, "magnitude", {});
    setParameter(magnitudeNode, "signalType", "spectralMagnitude");
    graph.addNode(std::move(magnitudeNode));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", {}));
    graph.addEdge({
            "magnitude",
            "out",
            "ifft",
            "mag",
            PortDomain::SpectralMagnitudeSignal
    });
    const Node* magnitude = graph.findNode("magnitude");
    REQUIRE(magnitude != nullptr);

    const auto configuration = NodeDspConfigurationFactory().create(
            AudioModuleRole::MeshSource,
            magnitude->parameters,
            magnitude->model,
            {},
            &graph,
            magnitude->id);
    const auto spectral = std::dynamic_pointer_cast<const TrimeshConfiguration>(
            configuration);

    REQUIRE(spectral != nullptr);
    REQUIRE(spectral->appliesSpectralRange);
    REQUIRE_FALSE(spectral->multiplicative);
}

TEST_CASE("Trimesh polarity is independent from downstream arithmetic",
        "[cycle-v2][runtime][configuration][spectral]") {
    struct Case {
        NodeKind operation;
        String polarity;
        bool multiplicative;
        bool bipolar;
    };
    const std::array<Case, 4> cases {{
            { NodeKind::Add, "unipolar", false, false },
            { NodeKind::Add, "bipolar", false, true },
            { NodeKind::Multiply, "unipolar", true, false },
            { NodeKind::Multiply, "bipolar", true, true }
    }};

    for (const Case& test : cases) {
        GraphNodeFactory factory;
        NodeGraph graph;
        Node magnitude = factory.createNode(NodeKind::TrilinearMesh, "magnitude", {});
        setParameter(magnitude, "signalType", "spectralMagnitude");
        setParameter(magnitude, "polarity", test.polarity);
        graph.addNode(std::move(magnitude));
        graph.addNode(factory.createNode(NodeKind::Fft, "base", {}));
        graph.addNode(factory.createNode(test.operation, "operation", {}));
        graph.addEdge({
                "base",
                "mag",
                "operation",
                "left",
                PortDomain::SpectralMagnitudeSignal
        });
        graph.addEdge({
                "magnitude",
                "out",
                "operation",
                "right",
                PortDomain::SpectralMagnitudeSignal
        });
        const Node* storedMagnitude = graph.findNode("magnitude");
        REQUIRE(storedMagnitude != nullptr);

        const auto configuration = NodeDspConfigurationFactory().create(
                AudioModuleRole::MeshSource,
                storedMagnitude->parameters,
                storedMagnitude->model,
                {},
                &graph,
                storedMagnitude->id);
        const auto spectral = std::dynamic_pointer_cast<const TrimeshConfiguration>(
                configuration);

        REQUIRE(spectral != nullptr);
        REQUIRE(spectral->appliesSpectralRange);
        REQUIRE(spectral->multiplicative == test.multiplicative);
        REQUIRE(spectral->bipolar == test.bipolar);
    }
}

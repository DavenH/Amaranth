#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

enum class AudioModuleRole {
    None,
    VoiceContext,
    ModulationSource,
    ModulationTriple,
    WaveSource,
    ImageSource,
    MeshSource,
    SpectralLayer,
    Fft,
    Ifft,
    Add,
    Multiply,
    Envelope,
    ImpulseResponse,
    Waveshaper,
    Unison,
    Reverb,
    Delay,
    Equalizer,
    StereoSplit,
    StereoJoin,
    Output,
    GenericProcessor
};

enum class PreviewModuleRole {
    None,
    VoiceContext,
    ModulationSource,
    Waveform,
    Image,
    MeshSurface,
    SpectrumMagnitude,
    SpectrumPhase,
    Envelope,
    ImpulseResponse,
    Waveshaper,
    ReverbSpectrogram,
    EqualizerResponse,
    SignalSpy,
    OutputMeters,
    Generic
};

enum class PreviewContract {
    None,
    AuthoritativeModel,
    RuntimeTap,
    Qualitative
};

enum class ParameterType {
    Boolean,
    Integer,
    Float,
    Choice,
    Text
};

class NodeModelCodec {
public:
    virtual ~NodeModelCodec() = default;

    virtual String schemaId() const = 0;
    virtual int currentVersion() const = 0;
    virtual NodeModelStatePtr createDefault() const = 0;
    virtual NodeModelStatePtr readJSON(const var& value, String& error) const = 0;
};

enum class ParameterImpact : uint32_t {
    None             = 0,
    Presentation     = 1 << 0,
    Preview          = 1 << 1,
    GraphSemantics   = 1 << 2,
    DspConfiguration = 1 << 3,
    ProcessorReset   = 1 << 4
};

ParameterImpact operator|(ParameterImpact left, ParameterImpact right);
bool hasImpact(ParameterImpact impacts, ParameterImpact impact);

struct ParameterConstraint {
    std::optional<double> minimum;
    std::optional<double> maximum;
    StringArray choices;
};

struct ParameterDefinition {
    String id;
    String label;
    ParameterType type { ParameterType::Text };
    String defaultValue;
    ParameterConstraint constraint;
    ParameterImpact impacts { ParameterImpact::None };
    bool persisted { true };
    std::function<String(const String&)> normalizer;

    bool accepts(const String& value) const;
    String normalized(const String& value) const;
};

enum class NodeExecutionTrait {
    ConfigurationOnly,
    ControlProducer,
    CycleGenerator,
    CoordinateTransform,
    SpectralTransform,
    OscillatorMaterializer,
    SampleBlockProcessor
};

struct NodeDefinition {
    String typeId;
    int version { 1 };
    NodeKind kind { NodeKind::GenericProcessor };
    String displayName;
    String subtitle;
    String helpText;
    String defaultInstanceIdPrefix;
    std::vector<Port> inputs;
    std::vector<Port> outputs;
    std::vector<ParameterDefinition> parameters;
    std::shared_ptr<const NodeModelCodec> modelCodec;
    bool allowsDynamicParameters {};
    AudioModuleRole audioRole { AudioModuleRole::None };
    PreviewModuleRole previewRole { PreviewModuleRole::None };
    PreviewContract previewContract { PreviewContract::None };
    bool executable {};
    bool previewable {};
    String cycle1Reference;
    NodeExecutionTrait executionTrait { NodeExecutionTrait::SampleBlockProcessor };
    NodeNaturalSize minimumPreviewSize { 190.f, 76.f };
    NodeNaturalSize fixedNaturalSize;
};

class NodeDefinitionRegistry {
public:
    static const NodeDefinitionRegistry& instance();

    const NodeDefinition* find(NodeKind kind) const;
    const NodeDefinition* find(const String& typeId) const;
    const ParameterDefinition* findParameter(NodeKind kind, const String& parameterId) const;
    const std::vector<NodeDefinition>& definitions() const { return nodeDefinitions; }

    String typeIdFor(NodeKind kind) const;
    NodeKind kindForTypeId(const String& typeId) const;
    void normalize(Node& node) const;

private:
    NodeDefinitionRegistry();

    std::vector<NodeDefinition> nodeDefinitions;
};

}

#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class NodeWorkspace;

class CycleV2AutomationInput {
public:
    using ComponentResolver = std::function<juce::Component*(const juce::String&)>;
    using CommandHandler = std::function<juce::var(const juce::var&)>;

    struct SemanticHandlers {
        CommandHandler setMorphSlider;
        CommandHandler setPrimaryAxis;
        CommandHandler toggleLink;
        CommandHandler setVertexParameter;
    };

    CycleV2AutomationInput(
            NodeWorkspace& workspace,
            ComponentResolver componentResolver,
            SemanticHandlers semanticHandlers);

    juce::var key(const juce::var& command);
    juce::var pointer(const juce::var& command);

private:
    NodeWorkspace& workspace;
    ComponentResolver resolveComponent;
    SemanticHandlers handlers;
};

}

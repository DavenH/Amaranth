#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class CycleV2AutomationAssertions {
public:
    using SnapshotProvider = std::function<juce::var()>;
    using ParameterReader = std::function<bool(
            const juce::String&,
            const juce::String&,
            juce::String&)>;

    CycleV2AutomationAssertions(
            SnapshotProvider snapshotProvider,
            ParameterReader parameterReader);

    juce::var assertState(const juce::var& command) const;
    juce::var assertNodeParameter(const juce::var& command) const;
    juce::var listAssertionPaths() const;

private:
    SnapshotProvider snapshot;
    ParameterReader readParameter;
};

}

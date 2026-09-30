#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class GraphDocumentReplacement {
public:
    enum class RequestResult {
        Rejected,
        Failed,
        Replaced,
        DecisionRequired
    };

    enum class Decision {
        Save,
        Discard,
        Cancel
    };

    enum class Resolution {
        Ignored,
        Failed,
        Canceled,
        Replaced,
        SaveRequired
    };

    using Replace = std::function<bool(const juce::File&)>;

    explicit GraphDocumentReplacement(Replace replaceCallback);

    RequestResult request(const juce::File& file, bool documentDirty);
    Resolution resolve(Decision decision);
    Resolution completeSave(bool succeeded);
    bool isPending() const { return pendingFile != juce::File(); }

private:
    Resolution replacePending();
    void clearPending();

    Replace replace;
    juce::File pendingFile;
    bool awaitingSave {};
};

}

#include "App/GraphDocumentReplacement.h"

#include <utility>

namespace CycleV2 {

GraphDocumentReplacement::GraphDocumentReplacement(Replace replaceCallback) :
        replace(std::move(replaceCallback)) {}

GraphDocumentReplacement::RequestResult GraphDocumentReplacement::request(
        const juce::File& file,
        bool documentDirty) {
    if (file == juce::File() || isPending()) {
        return RequestResult::Rejected;
    }
    if (!documentDirty) {
        return replace(file) ? RequestResult::Replaced : RequestResult::Failed;
    }

    pendingFile = file;
    return RequestResult::DecisionRequired;
}

GraphDocumentReplacement::Resolution GraphDocumentReplacement::resolve(Decision decision) {
    if (!isPending() || awaitingSave) {
        return Resolution::Ignored;
    }

    switch (decision) {
        case Decision::Save:
            awaitingSave = true;
            return Resolution::SaveRequired;

        case Decision::Discard:
            return replacePending();

        case Decision::Cancel:
            clearPending();
            return Resolution::Canceled;
    }

    return Resolution::Ignored;
}

GraphDocumentReplacement::Resolution GraphDocumentReplacement::completeSave(bool succeeded) {
    if (!isPending() || !awaitingSave) {
        return Resolution::Ignored;
    }
    if (!succeeded) {
        clearPending();
        return Resolution::Failed;
    }
    return replacePending();
}

GraphDocumentReplacement::Resolution GraphDocumentReplacement::replacePending() {
    const juce::File target = pendingFile;
    clearPending();
    return replace(target) ? Resolution::Replaced : Resolution::Failed;
}

void GraphDocumentReplacement::clearPending() {
    pendingFile = juce::File();
    awaitingSave = false;
}

}

#pragma once

#include <JuceHeader.h>

#include "InstallerManifest.h"

using namespace juce;

struct InstallSelection {
    String artifactId;
    String destinationId;
    String source;
    String destinationDirectory;
    bool enabled = false;
};

class InstallJob {
public:
    Result install(const File& packageFile, const Array<InstallSelection>& selections);
    const StringArray& getMessages() const { return messages; }

private:
    Result extractPackage(const File& packageFile, const File& tempDirectory);
    Result copySelection(const File& extractedRoot, const InstallSelection& selection);

    StringArray messages;
};
